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


#include "xp_mesg.h"
#include "xp_mesgp.h"
#include "xpassert.h"
#include "xp_trace.h"
#include <stddef.h>
#include <stdarg.h>

#ifdef DEBUG
int DebugMessages = 1;
#endif

/*-----------------------------------------------------------------------------
	stdarg random access
	
	The stdarg interface requires you to access the argument in order and
	specifying the types. We thus build an array first so that when we later
	want random access to an argument, we know the types of all the ones 
	before it.
-----------------------------------------------------------------------------*/

INLINE xpm_Integer
integer_argument (xpm_Args * args, int arg)
{
	va_list stack;
	xpm_Integer value;
	int i;
	
#ifdef VA_START_UGLINESS
	stack = args->stack;
#else
	va_start (stack, (*(args->start)));
#endif
#ifdef DEBUG
	XP_ASSERT (stack == args->stack);
#endif
	for (i = 0; i <= arg; i++) {
		if (args->types[i] == matInteger)
			value = va_arg (stack, xpm_Integer);
		else
			va_arg (stack, char*);
	}
	va_end (stack);
	
	XP_LTRACE(DebugMessages,1,("message: integer %i is %i\n", arg, value));
	return value;
}

INLINE char *
stack_string (xpm_Args * args, int arg)
{
	va_list stack;
	char * value;
	int i;
	
#ifdef VA_START_UGLINESS
	stack = args->stack;
#else
	va_start (stack, (*(args->start)));
#endif
#ifdef DEBUG
	XP_ASSERT (stack == args->stack);
#endif
	for (i = 0; i <= arg; i++) {
		if (args->types[i] == matInteger)
			va_arg (stack, xpm_Integer);
		else
			value = va_arg (stack, char*);
	}
	va_end (stack);
	
	XP_LTRACE(DebugMessages,1,("message: string %i is %s\n", arg, value));
	return value;
}

/*-----------------------------------------------------------------------------
	flow of control
	
	process_message runs through the format string (such as "open %1s with %2s"
	and sends normal characters through the "literal" output function and
	string and integer arguments though the "argument" function.
-----------------------------------------------------------------------------*/

static void
process_message (const char * format, OutputStream * o)
{
	const char * current = format;
	char c = *current;
	
	while (c) {
		if (c == '%') {
			c = *++current;
			XP_ASSERT (c);
			if (c == '%') {
				(o->writeLiteral) (o, '%');
				c = *++current;
			}
			else {
				int i = (c - '1');
				XP_ASSERT (0 <= i && i < MessageArgsMax);
				c = *++current;
				XP_ASSERT (c);
				(o->writeArgument) (o, c, i);
				c = *++current;
			}
		}
		else {
			(o->writeLiteral) (o, c);
			c = *++current;
		}
	}
}

/*-----------------------------------------------------------------------------
	ScanStream runs through the format string and builds the stdarg random
	access table.
-----------------------------------------------------------------------------*/

typedef struct ScanStream_ {
	OutputStream	o;
} ScanStream;

static void 
xpm_scan_literal (OutputStream * o, char c)
{
	ScanStream * s = (ScanStream *) o;
}

static void
xpm_scan_argument (OutputStream * o, char c, int i)
{
	ScanStream * s = (ScanStream *) o;
	if (c == 's') {
		s->o.args->sizes[i] = sizeof (char*);
		s->o.args->types[i] = matString;
	}
	else if (c == 'i') {
		s->o.args->sizes[i] = sizeof (xpm_Integer);
		s->o.args->types[i] = matInteger;
	}
	else
		XP_ABORT(("message: invalid specifier `%c'\n", c));
}

static void
xpm_ScanStack (const char * format, xpm_Args * args)
{
	ScanStream s;
	s.o.writeLiteral = & xpm_scan_literal;
	s.o.writeArgument = & xpm_scan_argument;
	s.o.args = args;
	
	process_message (format, & s.o);
}

/*-----------------------------------------------------------------------------
	CountStream counts the total number of characters in the message.
-----------------------------------------------------------------------------*/

typedef struct CountStream_ {
	OutputStream	o;
	int				total;
} CountStream;

static void 
xpm_count_literal (OutputStream * o, char c)
{
	CountStream * s = (CountStream *) o;
	s->total ++;
}

static void
xpm_count_argument (OutputStream * o, char c, int i)
{
	CountStream * s = (CountStream *) o;
	if (c == 's') {
		char * value = stack_string (o->args, i);
		int len = value? strlen (value): 0;
		s->total += len;
	}
	else if (c == 'i') {
		s->total += 10;
	}
}

static int
xpm_MessageLen (const char * format, xpm_Args * args)
{
	CountStream s;
	s.o.writeLiteral = & xpm_count_literal;
	s.o.writeArgument = & xpm_count_argument;
	s.o.args = args;
	s.total = 0;
	
	xpm_ScanStack (format, args);
	process_message (format, & s.o);
	
	return s.total + 1 /* NULL */;
}

/*-----------------------------------------------------------------------------
	Public API
	These just build stdarg structures and call the internal functions.
-----------------------------------------------------------------------------*/

int
XP_MessageLen (const char * format, ...)
{
	xpm_Args args;
	int len;
	
#ifdef VA_START_UGLINESS
	va_start (args.stack, format);
#endif
	args.start = &format;
	len = xpm_MessageLen (format, &args);
#ifdef VA_START_UGLINESS
	va_end (args.stack);
#endif
	
	return len;
}

void
XP_Message (char * buffer, int bufferLen, const char * format, ...)
{
/*
	This is a positional format string function. It's almost like
	sprintf, except that sprintf takes the format arguments in the
	order they are pushed on the stack. This won't work for
	il8n purposes. In other languages the arguments may have to be
	used in a different order. Therefore we need a format string
	which specifies the argument explicitly (say by number).
	
	sprintf fails us here:
		English: format = "open %1s with %2s"
		Japanese: format = "with %2s open %1s"
	printf (format, "foo.gif", "LView") ->
		English: "open foo.gif with LView"
		Japanese: "with foo.gif open LView" (wrong)
	
	XP_Message will work like this:
		English: format = "open %1s with %2s"
		Japanese: format = "with %2s open %1s"
	XP_Message (format, "foo.gif", "LView") ->
		English: "open foo.gif with LView"
		Japanese: "with LView open foo.gif" (arguments in correct positions)
	
	The syntax is: %NT, N = number of argument, T = type (only s and i
	supported). So %1s is the first argument as a string, %2i is the second 
	argument as a number. No other printf modifiers (field widths, etc) are 
	supported. It is legal to reference the same argument multiple times.
	
	%% outputs %.
	There is a maximum of 9 arguments.
*/
}

const char *
XP_StaticMessage (const char * format, ...)
{
	return NULL;
}

