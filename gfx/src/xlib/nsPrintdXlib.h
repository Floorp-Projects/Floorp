/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Vino Fernando Crescini <vino@igelaus.com.au>
 */

#ifndef nsPrintdXlib_h___
#define nsPrintdxlib_h___

#include <limits.h>

PR_BEGIN_EXTERN_C

#ifndef NS_LEGAL_SIZE
#define NS_LETTER_SIZE    0
#define NS_LEGAL_SIZE     1
#define NS_EXECUTIVE_SIZE 2
#define NS_A4_SIZE        3
#endif

#ifndef PATH_MAX
#ifdef _POSIX_PATH_MAX
#define PATH_MAX		 _POSIX_PATH_MAX
#else
#define PATH_MAX		 256
#endif
#endif

typedef struct unixprdata {
  PRBool toPrinter;          /* If PR_TRUE, print to printer */
  PRBool fpf;                /* If PR_TRUE, first page first */
  PRBool grayscale;          /* If PR_TRUE, print grayscale */
  int size;                  /* Paper size e.g., SizeLetter */
  char command[PATH_MAX];    /* Print command e.g., lpr */
  char path[PATH_MAX];       /* If toPrinter = PR_FALSE, dest file */
  PRBool cancel;		    /* If PR_TRUE, user cancelled */
  float left;		         /* left margin */
  float right;		         /* right margin */
  float top;		         /* top margin */
  float bottom;		    /* bottom margin */
} UnixPrData;

void UnixPrDialog(UnixPrData *prData);

PR_END_EXTERN_C

#endif                       /* nsPrintdXlib_h___ */

