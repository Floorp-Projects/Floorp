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

#ifndef __cgi_h_
#define __cgi_h_

/*
** CGI assist library. Portability layer for writing correctly behaving
** CGI programs.
*/
#include "ds.h"

XP_BEGIN_PROTOS

/*
** Read in the input, generating a single long string out of it.  CGI
** programs normally get the value of various forms elements as input.
*/
extern char *CGI_GatherInput(FILE *in);

/*
** Given a null terminated string, compress it in place, converting
** "funny characters" into their ascii equivalent. Maps "+" to space and
** %xx to the binary version of xx, where xx is a pair of hex digits.
*/
extern void CGI_CompressString(char *s);

/*
** Convert a string into an argument vector. This seperates the incoming
** string into pieces, and calls CGI_CompressString to compress the
** pieces. This allocates memory for the return value only.
*/
extern char **CGI_ConvertStringToArgVec(char *string, int *argcp);

/*
** Look for the variable called "name" in the argv. Return a pointer to
** the value portion of the variable if found, zero otherwise.  this does
** not malloc memory.
*/
extern char *CGI_GetVariable(char *name, int argc, char **argv);

/* Return non-zero if the variable string is not empty */
#define CGI_IsEmpty(var) (!(var) || ((var)[0] == 0))

/*
** Return true if the server that started the cgi running is using
** security (https).
*/
extern DSBool CGI_IsSecureServer(void);

/*
** Concatenate strings to produce a single string.
*/
extern char *CGI_Cat(char *, ...);

/* Escape a string, cgi style */
char *CGI_Escape(char *in);

XP_END_PROTOS

#endif /* __cgi_h_ */
