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
/*
** This is a feeble attempt at creating a BSD-style echo command
** for use on platforms that have only a Sys-V echo.  This version
** supports the '-n' flag, and will not interpret '\c', etc.  As
** of this writing this is only needed on HP-UX and AIX 4.1.
** --briano.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(XP_OS2)
#include <unistd.h>
#endif

#ifdef SUNOS4
#include "sunos4.h"
#endif

void main(int argc, char **argv)
{
	short numargs = argc;
	short newline = 1;

	if (numargs == 1)
	{
		exit(0);
	}

	if (strcmp(*++argv, "-n") == 0)
	{
		if (numargs == 2)
		{
			exit(0);
		}
		else
		{
			newline = 0;
			numargs--;
			argv++;
		}
	}

	while (numargs > 1)
	{
		fprintf(stdout, "%s", *argv++);
		numargs--;
		if (numargs > 1)
		{
			fprintf(stdout, " ");
		}
	}

	if (newline == 1)
	{
		fprintf(stdout, "\n");
	}

	exit(0);
}
