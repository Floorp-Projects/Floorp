/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
// CommandLine.cpp
//
// Scot M. Silver
//

/* fileman.c -- A tiny application which demonstrates how to use the
   GNU Readline library.  This application interactively allows users
   to manipulate files and their modes. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C"
{
#include "readline.h"
#include "history.h"
}

#include "Commands.h"

bool gCommandLineLoopActive = false;

const char *
completeCommand(char* inText, int inState);

const int kMonitorMajorVersion = 0;
const int kMonitorMinorVersion = 1;

void version(char*)
{
	printf(	"Furmon - v%d.%d\n"
			"Copyright 1997-8 Netscape Communications Corp.\n"
			"All Rights Reserved\n"
			"\n"
			"Readline Library property of FSF\n", kMonitorMajorVersion, kMonitorMinorVersion);
}

bool gEndCommandLine;


// Commands understood
struct MonitorCommand
{
	const char* mName;			// name
	void  (*mFunc)(char*);		// function which implements this
	const char* mHelp;			// help string
	
	// list of all commands available
	static MonitorCommand	mAllCommands[];

	static MonitorCommand*	find(const char* inKeyString) { return(findFrom(mAllCommands, inKeyString, strlen(inKeyString))); }
	static MonitorCommand*	findFrom(MonitorCommand* inLastCommand, const char* inKeyString, int inMaxLen = 0);
	
	void					execute(char* inArguments) { (*mFunc)(inArguments); }
	
};

void  help(char*);

MonitorCommand MonitorCommand::mAllCommands[] =
{
	{ "help",		help,				"Print out this message" },
	{ "?",			help,				"Print out this message" },
	{ "version",	version,			"Print version information" },
	{ "step-in",	stepIn,				"Step into" },
	{ "si",			stepIn,				"Step into" },
	{ "step-over",	stepOver,			"Step over" },
	{ "so",			stepOver,			"Step over" },
	{ "q",			quit,				"Quit" },
	{ "continue",	go,					"Resume current thread"},
	{ "go",			go,					"Resume current thread"},
	{ "il",			disassemble,		"Disassemble"},
	{ "disass",		disassemble,		"Disassemble"},
	{ "br",			breakExecution,		"Set an execution breakpoint"},
	{ "bre",		breakExecution,		"Set an execution breakpoint"},
	{ "run",		run,				"Run an application"},
	{ "allthreads",	printThreads,		"Print all threads that were or are"},
	{ "thread",		printThisThread,	"Print current thread"},
	{ "sc",			printThreadStack,	"Print current thread stack"},
	{ "where",		printThreadStack,	"Print current thread stack"},
	{ "rc",			runClass,			"Run main method of a given class"},
	{ "swt",		switchThread,		"Switch to thread id"},
	{ "cv",			connectToVM,		"Force reconnection to VM"},
	{ "reg",		printIntegerRegs,	"Print integer registers"},
	{ "allreg",		printAllRegs,		"Print all registers"},
	{ "kill",		kill,				"Kill currently running program"}
};

void help(char*)
{
	MonitorCommand*	lastCommand = MonitorCommand::mAllCommands + sizeof(MonitorCommand::mAllCommands) / sizeof(MonitorCommand);
	MonitorCommand*	firstCommand = MonitorCommand::mAllCommands;
	MonitorCommand*	curCommand;

	for (curCommand = firstCommand; curCommand < lastCommand; curCommand++)
		printf("%-10s %.40s\n", curCommand->mName, curCommand->mHelp);
}


MonitorCommand* MonitorCommand::
findFrom(MonitorCommand* inCommand, const char* inKeyString, int inMaxLen)
{
	MonitorCommand*	lastCommand = mAllCommands + sizeof(mAllCommands) / sizeof(MonitorCommand);
	MonitorCommand*	firstCommand = inCommand ? inCommand : mAllCommands;
	MonitorCommand*	curCommand;

	for (curCommand = firstCommand; curCommand < lastCommand; curCommand++)
	{
		if (inMaxLen)
		{
			if (!strncmp(inKeyString, curCommand->mName, inMaxLen))
				return (curCommand);
		}
		else
		{
			if (!strcmp(inKeyString, curCommand->mName))
				return (curCommand);
		}
	}

	return (NULL);
}	


int
valid_argument (char*, char*arg);

void
initialize_readline ();

void
stripwhite(char* string);

void
execute_line(char* line);

extern "C" char **
fileman_completion(char* text, int start, int end);


/* The name of this program, as taken from argv[0]. */
char *progname;


int
commandLineLoop(int argc, char**argv)
{
	initialize_readline ();

	progname = argv[0];

	version(NULL);
	char*	lastLine = NULL;

	gCommandLineLoopActive = true;

	/* Loop reading and executing lines until the user quits. */
	while (!gEndCommandLine)
    {
		char *line;

		line = readline("> ");

		if (!line)
			gEndCommandLine = true;		/* Encountered EOF at top level. */
		else
		{
			/*	Remove leading and trailing whitespace from the line.
				Then, if there is anything left, add it to the history list
				and execute it. */
			stripwhite (line);

			// try to hang on to the lastLine, if the user enters an empty
			// line then do the last command
			if (*line)
			{
				add_history(line);
				execute_line(line);
				if (lastLine)
					free(lastLine);
				lastLine = strdup(line);
				
		    }
			else if (lastLine)
			{
				add_history(lastLine);
				execute_line(lastLine);
			}
		}

     if (line)
		free (line);
    }

	return 0;
}

/* Execute a command line. */
void
execute_line(char* line)
{
	int i;
	MonitorCommand *command;
	char *word;

	/* Isolate the command word. */
	i = 0;
	while (line[i] && !whitespace (line[i]))
		i++;

	word = line;

	// FIX-ME this is destructive and this will fuck us
	// when we want to evaluate expressions
	if (line[i])
		line[i++] = '\0';

	command = MonitorCommand::find(word);

	if (!command)
	{
		unhandledCommandLine(line);
		return;
	}

	// skip to next non-whitespace character to find argument
	while (whitespace (line[i]))
		i++;

	word = line + i;

	// Call the function
	command->execute(word);
}


/* Strip whitespace from the start and end of STRING. */
void
stripwhite (char* string)
{
  register int i = 0;

  while (whitespace (string[i]))
    i++;

  if (i)
    strcpy (string, string + i);

  i = strlen (string) - 1;

  while (i > 0 && whitespace (string[i]))
    i--;

  string[++i] = '\0';
}

/* **************************************************************** */
/*                                                                  */
/*                  Interface to Readline Completion                */
/*                                                                  */
/* **************************************************************** */

/* Tell the GNU Readline library how to complete.  We want to try to complete
   on command names if this is the first word in the line, or on filenames
   if not. */
void
initialize_readline ()
{
  /* Allow conditional parsing of the ~/.inputrc file. */
  rl_readline_name = "Furmon";

  /* Tell the completer that we want a crack first. */
  rl_attempted_completion_function = fileman_completion;
}

/* Attempt to complete on the contents of TEXT.  START and END show the
   region of TEXT that contains the word to complete.  We can use the
   entire line in case we want to do some simple parsing.  Return the
   array of matches, or NULL if there aren't any. */
extern char  * 
command_generator (char* text, int state);


char **
fileman_completion (char* text, int start, int end)
{
  char **matches;
  matches = NULL;

  /* If this word is at the start of the line, then it is a command
     to complete.  Otherwise it is the name of a file in the current
     directory. */
  if (start == 0)
    matches = completion_matches (text, completeCommand);

  return (matches);
}

MonitorCommand* gLastCommand = NULL;

/* Generator function for command completion.  STATE lets us know whether
   to start from scratch; without any state (i.e. STATE == 0), then we
   start at the top of the list. */
const char *
completeCommand(char* inText, int inState)
{
 	if (!inState)
		gLastCommand = NULL;

	gLastCommand = MonitorCommand::findFrom(gLastCommand ? gLastCommand + 1 : NULL, inText, strlen(inText));

	return (gLastCommand ? gLastCommand->mName : NULL);
}

