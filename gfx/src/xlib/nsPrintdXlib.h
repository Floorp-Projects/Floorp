/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vino Fernando Crescini <vino@igelaus.com.au>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsPrintdXlib_h___
#define nsPrintdxlib_h___

#include <limits.h>

PR_BEGIN_EXTERN_C

#ifndef NS_LEGAL_SIZE
#define NS_LETTER_SIZE    0
#define NS_LEGAL_SIZE     1
#define NS_EXECUTIVE_SIZE 2
#define NS_A4_SIZE        3
#define NS_A3_SIZE        4

#define NS_PORTRAIT  0
#define NS_LANDSCAPE 1
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
  int orientation;           /* Orientation e.g. Portrait */
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

