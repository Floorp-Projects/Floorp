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


/*-----------------------------------------------------------------------------
	Private Typesdefs, Globals, Etc
-----------------------------------------------------------------------------*/

#if 1
# define VA_START_UGLINESS
#endif

#include <stdarg.h>

#ifndef _XP_Message_Private_
#define _XP_Message_Private_

#define MessageArgsMax 9

typedef enum { matString, matInteger, matLiteral }  xpm_ArgType;
typedef struct xpm_Args_ xpm_Args;

typedef long xpm_Integer;

#if !defined(DEBUG) && (defined(__cplusplus) || defined(__gcc))
# ifndef INLINE
#  define INLINE inline
# endif
#else
# define INLINE static
#endif

struct xpm_Args_ {
	int				sizes [MessageArgsMax];
	xpm_ArgType		types [MessageArgsMax];
#ifdef VA_START_UGLINESS
	va_list 		stack;
#endif
	const char ** 	start;
	int				max;
};

/*-----------------------------------------------------------------------------
	streams interface to processing format strings
-----------------------------------------------------------------------------*/

typedef struct OutputStream_ OutputStream;

typedef void 
WriteLiteral (OutputStream * o, char c);

typedef void
WriteArgument (OutputStream * o, char c, int argument);

struct OutputStream_ {
	WriteLiteral *	writeLiteral;
	WriteArgument*	writeArgument;
	xpm_Args *		args;
};

#endif /* _XP_Message_Private_ */

