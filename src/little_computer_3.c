#include <stdio.h>
#include <stdint.h>

#define WORD_SIZE 16
#define N_REGISTERS 10
#define MAX_ADDRESSABLE (1 << WORD_SIZE)
#define END_OF_USER_SPACE 0xFDFF
#define USER_SPACE_START 0x3000

uint16_t memory[MAX_ADDRESSABLE];
uint16_t registers[N_REGISTERS];

enum
{
	R_0,
	R_1,
	R_2,
	R_3,
	R_4,
	R_5,
	R_6,
	R_7,
	R_PC,
	R_PSR
};

enum
{
	INSTR_BR,
	INSTR_ADD,
	INSTR_LD,
	INSTR_ST,
	INSTR_JSR,
	INSTR_AND,
	INSTR_LDR,
	INSTR_STR,
	INSTR_RTI,
	INSTR_NOT,
	INSTR_LDI,
	INSTR_STI,
	INSTR_JMP,
	INSTR_ILL,
	INSTR_LEA,
	INSTR_TRAP
};

enum
{
	TRAP_GETC = 0x20,
	TRAP_OUT,
	TRAP_PUTS,
	TRAP_IN,
	TRAP_PUTSP,
	TRAP_HALT
};

enum
{
	COND_NEG = 4,
	COND_ZERO = 2,
	COND_POS = 1
};

uint16_t sign_extend(uint16_t value, int n)
{
	uint16_t mask = 1 << (n - 1);
	return (value ^ mask) - mask;
}

void update_condition_code(uint16_t dr)
{
	if (dr < 0)
	{
		registers[R_PSR] = (registers[R_PSR] & 0x8000) + COND_NEG;
	}
	else if (dr == 0)
	{
		registers[R_PSR] = (registers[R_PSR] & 0x8000) + COND_ZERO;
	}
	else
	{
		registers[R_PSR] = (registers[R_PSR] & 0x8000) + COND_POS;
	}
}


uint16_t switch_endianness(uint16_t instruction)
{
	return (instruction << (WORD_SIZE/2)) | (instruction >> (WORD_SIZE/2));
}

uint16_t get_bits_between(uint16_t instruction, int n, int k)
{
	return (instruction & ((1 << (n + 1)) - 1)) >> k;
}

int execute(uint16_t instruction, uint16_t *arguments)
{
	uint16_t opcode = get_bits_between(instruction, 15, 12);

	++registers[R_PC];

	switch (opcode)
	{
		case INSTR_ADD:
		case INSTR_AND:
		case INSTR_NOT:
		{
			arguments[0] = get_bits_between(instruction, 11, 9);
			arguments[1] = get_bits_between(instruction, 8, 6);

			if (get_bits_between(instruction, 5, 5))
			{
				arguments[2] = sign_extend(get_bits_between(instruction, 4, 0), 5);
			}
			else
			{
				arguments[2] = get_bits_between(instruction, 2, 0);
			}
			break;
		}
		case INSTR_BR:
		{
			arguments[2] = get_bits_between(registers[R_PSR], 2, 0);
		}
		case INSTR_LD:
		case INSTR_LDI:
		case INSTR_LEA:
		case INSTR_ST:
		case INSTR_STI:
		{
			arguments[0] = get_bits_between(instruction, 11, 9);
			arguments[1] = get_bits_between(instruction, 8, 0);

			break;
		}
		case INSTR_LDR:
		case INSTR_STR:
		{
			arguments[0] = get_bits_between(instruction, 11, 9);
			arguments[1] = get_bits_between(instruction, 8, 6);
			arguments[2] = get_bits_between(instruction, 5, 0);
			break;
		}
		case INSTR_JSR:
		{
			if (get_bits_between(instruction, 11, 11))
			{
				arguments[0] = get_bits_between(instruction, 10, 0);
			}
			else
			{
				arguments[0] = get_bits_between(instruction, 8, 6);
			}
			break;
		}
		case INSTR_JMP:
		{
			arguments[0] = get_bits_between(instruction, 8, 6);
			break;
		}
		case INSTR_TRAP:
		{
			arguments[0] = get_bits_between(instruction, 7, 0);
			break;
		}
	}

	switch (opcode)
	{
		case INSTR_BR:
		{
			if (arguments[0] & arguments[2])
			{
				registers[R_PC] += sign_extend(arguments[1], 9);
			}
			break;
		}		
		case INSTR_ADD:
		{
			if (get_bits_between(instruction, 5, 5))
			{
				registers[arguments[0]] = registers[arguments[1]] + sign_extend(arguments[2], 5);
			}
			else
			{
				registers[arguments[0]] = registers[arguments[1]] + registers[arguments[2]];
			}
			update_condition_code(registers[arguments[0]]);
			break;
		}
		case INSTR_LD:
		{
			uint16_t pcoffset9 = sign_extend(arguments[1], 9);
			registers[arguments[0]] = switch_endianness(memory[registers[R_PC] + pcoffset9]);
			update_condition_code(registers[arguments[0]]);
			break;
		}
			
		case INSTR_ST:
		{
			memory[registers[R_PC] + sign_extend(arguments[1], 9)] = switch_endianness(registers[arguments[0]]);
			break;
		}
		case INSTR_JSR:
		{
			if (get_bits_between(instruction, 11, 11))
			{
				registers[R_7] = registers[R_PC];
				registers[R_PC] += sign_extend(arguments[0], 11);
			}
			else
			{
				registers[R_7] = registers[R_PC];
				registers[R_PC] = registers[arguments[0]];
			}
			break;
		}
		case INSTR_AND:
		{
			if (get_bits_between(instruction, 5, 5))
			{
				registers[arguments[0]] = registers[arguments[1]] & sign_extend(arguments[2], 5);
			}
			else
			{
				registers[arguments[0]] = registers[arguments[1]] & registers[arguments[2]];
			}
			update_condition_code(registers[arguments[0]]);
			break;
		}
		case INSTR_LDR:
		{
			registers[arguments[0]] = switch_endianness(memory[registers[arguments[1]] + sign_extend(arguments[2], 6)]);
			update_condition_code(registers[arguments[0]]);
			break;
		}
		case INSTR_STR:
		{
			memory[registers[arguments[1]] + sign_extend(arguments[2], 6)] = switch_endianness(registers[arguments[0]]);
			break;
		}
		case INSTR_RTI:
		{
			//to-do
			break;
		}
		case INSTR_NOT:
		{
			registers[arguments[0]] = ~registers[arguments[1]];
			update_condition_code(registers[arguments[0]]);
			break;

		}
		case INSTR_LDI:
		{
			uint16_t pointer = switch_endianness(memory[registers[R_PC] + sign_extend(arguments[1], 9)]);
			registers[arguments[0]] = switch_endianness(memory[pointer]);
			update_condition_code(registers[arguments[0]]);
			break;
		}
		case INSTR_STI:
		{
			uint16_t pointer = switch_endianness(memory[registers[R_PC] + sign_extend(arguments[1], 9)]);
			memory[pointer] = switch_endianness(registers[arguments[0]]);
			break;
		}
		case INSTR_JMP:
		{
			registers[R_PC] = registers[arguments[0]];
			break;
		}
		case INSTR_ILL:
		{
			//to-do
			break;
		}
		case INSTR_LEA:
		{
			registers[arguments[0]] = registers[R_PC] + sign_extend(arguments[1], 9);
			update_condition_code(registers[arguments[0]]);
			break;
		}
		case INSTR_TRAP:
		{
			switch (arguments[0])
			{
				case TRAP_PUTS:
				{
					printf("Puts called\n");
					break;
				}
				case TRAP_GETC:
				{
					break;
				}
				case TRAP_HALT:
				{
					printf("Halting processor\n");
					return 0;
				}					
			}
			break;
		}
	}

	return 1;
}

int main(int argc, char **argv)
{
	FILE *file = fopen("./assembly.obj", "r");

	uint16_t program_start;
	fread(&program_start, 2, 1, file);
	program_start = switch_endianness(program_start);

	int available_memory = END_OF_USER_SPACE - program_start;

	registers[R_PC] = program_start;
	registers[R_PSR] = COND_ZERO;
	
	size_t words_read = fread(memory + program_start, 2, available_memory, file);

	printf("%ld 16-bit words read into memory\n", words_read);
	
	//to-do forbid start in supervisor space
	//to-do exit if image cannot fit into memory (check if eof reached)
	//to-do interrupts
	//to-do trap routines
	//to-do exceptions
	//to-do clean up main

	int running = 1;

	uint16_t arguments[3];

	while (running)
	{

		uint16_t instruction = switch_endianness(memory[registers[R_PC]]);

		running = execute(instruction, arguments);
	}

	fclose(file);

	return 0;
}