/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/* Original Code: Syd Logan (syd@netscape.com) 3/12/99 */

#ifndef nsPrintdGTK_h___
#define nsPrintdGTK_h___

#include <limits.h>

PR_BEGIN_EXTERN_C

/* stolen from nsPostScriptObj.h. needs to be put somewhere else that
   both ps and gtk can see easily */

#ifndef NS_LEGAL_SIZE
#define NS_LETTER_SIZE    0
#define NS_LEGAL_SIZE     1
#define NS_EXECUTIVE_SIZE 2
#define NS_A4_SIZE        3
#endif

#ifdef NTO
// XXX Perhaps an NSPR macro would be a better solution.
#define PATH_MAX _POSIX_PATH_MAX
#endif

typedef struct unixprdata {
        PRBool toPrinter;          /* If PR_TRUE, print to printer */
        PRBool fpf;                /* If PR_TRUE, first page first */
        PRBool grayscale;          /* If PR_TRUE, print grayscale */
        int size;                   /* Paper size e.g., SizeLetter */
        char command[ PATH_MAX ];   /* Print command e.g., lpr */
        char path[ PATH_MAX ];      /* If toPrinter = PR_FALSE, dest file */
	PRBool cancel;		    /* If PR_TRUE, user cancelled */
	float left;		    /* left margin */
	float right;		    /* right margin */
	float top;		    /* top margin */
	float bottom;		    /* bottom margin */
} UnixPrData;

void UnixPrDialog(UnixPrData *prData);

PR_END_EXTERN_C

#endif /* nsPrintdGTK_h___ */
