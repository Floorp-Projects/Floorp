/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* */
/* mkcommands.c - munge the commands file and output either a .c file, .h file, or both.
   Created: Chris Toshok, toshok@netscape.com, 18-Feb-1997 
*/



#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define LINEBUFFER_SIZE 1000

char *munge_filename = NULL;
char *header_filename = "commands.h";
char *table_filename = "commands.c";
int header_munging = 0;
int table_munging = 0;

typedef struct {
	char *command;
	char *string;
} CommandInfo;

CommandInfo *commands = NULL;
int num_commands = 0;
int alloced_commands = 0;

void
write_file_header(FILE *fp)
{
	fprintf(fp,
			"/* AUTOMATICALLY GENERATED FILE.  Do not edit!  Instead,\n"
			"   modify the %s file, and use mkcommands to regenerate\n"
			"   this file. */\n\n", munge_filename);
}

void
add_command(char *command, char *string)
{
	if (num_commands == alloced_commands)
		{
			alloced_commands +=300;
			commands = realloc(commands, sizeof(CommandInfo) * alloced_commands);
		}

	commands[num_commands].command = strdup(command);
	commands[num_commands].string = strdup(string);
	
	num_commands++;
}

static int
compare_function(const void *c1, const void *c2)
{
	CommandInfo *com1 = (CommandInfo*)c1;
	CommandInfo *com2 = (CommandInfo*)c2;

	return strcmp(com1->string, com2->string);
}

void
sort_commands(void)
{
	/* sort the commands by their string */
	qsort(commands, num_commands, sizeof(CommandInfo), compare_function);
}

void
output_header(FILE *fp)
{
	int table_position;
	int i;

	write_file_header(fp);

	fprintf (fp, "extern char _XfeCommands[];\n");
	fprintf (fp, "extern int _XfeNumCommands;\n");
	fprintf (fp, "extern int _XfeCommandIndices[];\n");

	table_position = 0;
	for (i = 0; i < num_commands; i ++)
		{
			fprintf (fp, "#define %s ((char*)&_XfeCommands[ %d ])\n", commands[i].command, table_position);
			
			table_position += strlen(commands[i].string) + 1;
		}

	fclose(fp);
}

void
output_table(FILE *fp)
{
	int i;
	int table_position;

	write_file_header(fp);

	fprintf (fp,
			 "int _XfeNumCommands = %d;\n", num_commands);

	fprintf (fp,
			 "int _XfeCommandIndices[] = {\n");

	table_position = 0;
	for (i = 0; i < num_commands; i ++)
		{
			fprintf (fp, "\t%d,", table_position);
			if (i % 4 == 0) fprintf (fp, "\n");
			table_position += strlen(commands[i].string) + 1;
		}

	fprintf (fp,
			 "};\n");

	fprintf (fp, 
			 "char _XfeCommands[] = \n");

	table_position = 0;
	for (i = 0; i < num_commands; i ++)
		{
			fprintf (fp, "\t/* %10d */ \"%s\\0\"\n", table_position, commands[i].string);

			table_position += strlen(commands[i].string) + 1;
		}

	/* write out the closing semicolon. */
	fprintf (fp, ";\n");
	fclose(fp);
}

void
munge_file(void)
{
	FILE *munge_fp = 0;
	FILE *table_fp = 0;
	FILE *header_fp = 0;
	static char line_buf[LINEBUFFER_SIZE + 1];

	if ((munge_fp = fopen(munge_filename, "r")) == NULL)
		{
			fprintf(stderr, "couldn't open '%s' for reading\n", munge_filename);
			exit(-1);
		}

	if (header_munging)
		if ((header_fp = fopen(header_filename, "w")) == NULL)
			{
				fprintf(stderr, "couldn't open '%s' for writing\n", header_filename);
				exit(-1);
			}
	
	if (table_munging)
		if ((table_fp = fopen(table_filename, "w")) == NULL)
			{
				fprintf(stderr, "couldn't open '%s' for writing\n", table_filename);
				exit(-1);
			}

	while (fgets(line_buf, LINEBUFFER_SIZE, munge_fp))
		{
			if (line_buf[0] == '#') /* comment lines. */
				{
					continue;
				}
			else /* non comment lines */
				{
					char command[100], string[100];

					command[0] = string[0] = 0;

					sscanf(line_buf, " %s %s ", command, string);

					if (!command[0] || !string[0])
						continue;

					add_command(command, string);

				}
		}

	fclose(munge_fp);

	sort_commands();

	if (header_munging)
		output_header(header_fp);

	if (table_munging)
		output_table(table_fp);
}

void
usage(void)
{
	fprintf (stderr,
			 "Usage:  mkcommands [-header <header_file_name>] [-table <c_file_name>] <command_file>\n"
			 "<header_file_name> is the file written out containing the #defines for the strings.\n"
			 "<c_file_name> is the file written out containing the compilable string table.\n");
}

int
main(int argc, char **argv)
{
	int n = 0;

	for (n = 1; n < argc; n ++) 
		{
			if (strcmp(argv[n], "-header") == 0)
				{
					if (argc < n+2)
						{
							fprintf(stderr, "-header requires an argument\n");
							usage();
							return 2;
						}

					header_filename = strdup(argv[n+1]);
					header_munging = 1;
					n++;
				}
			else if (strcmp(argv[n], "-table") == 0)
				{
					if (argc < n+2)
						{
							fprintf(stderr, "-table requires an argument\n");
							usage();
							return 2;
						}

					table_filename = strdup(argv[n+1]);
					table_munging = 1;
					n++;
				}
			else
				{
					if (munge_filename == NULL)
						{
							munge_filename = strdup(argv[n]);
						}
					else
						{
							fprintf(stderr, "Unknown flag %s\n", argv[n]);
							usage();
							return 2;
						}
				}
		}

	if (!munge_filename)
		{
			fprintf (stderr, "You must specify a command file on the command line.\n");
			usage();
			return 2;
		}

	if (!table_munging && !header_munging)
		{
			fprintf (stderr, "Um, like at least one of -table or -header must be specified on the command line\n");
			usage();
			return 2;
		}

	/* allocate the initial command table, with 200 elements */
	alloced_commands = 200;
	commands = malloc(sizeof(CommandInfo) * alloced_commands);
	if (!commands)
		{
			fprintf (stderr, "Unable to mallog. You lose\n");
			return 2;
		}

	munge_file();
	return 0;
}


