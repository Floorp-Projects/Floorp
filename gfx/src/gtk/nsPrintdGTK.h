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

#include <limits.h>

enum { SizeLetter, SizeLegal, SizeExecutive, SizeA4 };

typedef struct unixprdata {
        PRBool toPrinter;          /* If PR_TRUE, print to printer */
        PRBool fpf;                /* If PR_TRUE, first page first */
        PRBool grayscale;          /* If PR_TRUE, print grayscale */
        int size;                   /* Paper size e.g., SizeLetter */
        char command[ PATH_MAX ];   /* Print command e.g., lpr */
        char path[ PATH_MAX ];      /* If toPrinter = PR_FALSE, dest file */
	PRBool cancel;		    /* If PR_TRUE, user cancelled */
	FILE *stream;		    /* File or Printer */
} UnixPrData;

void UnixPrDialog(UnixPrData *prData);
