/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 */


/*	csid.h	*/

#ifndef _CSID_H_
#define _CSID_H_

/* Codeset type */
#define SINGLEBYTE   0x0000 /* 0000 0000 0000 0000 =    0 */
#define MULTIBYTE    0x0100 /* 0000 0001 0000 0000 =  256 */

/* Code Set IDs */
/* CS_DEFAULT: used if no charset param in header */
/* CS_UNKNOWN: used for unrecognized charset */

                    /* type                  id   */
#define CS_DEFAULT    (SINGLEBYTE         |   0) /*    0 */
#define CS_UTF8       (MULTIBYTE          |  34) /*  290 */
#define CS_UNKNOWN    (SINGLEBYTE         | 255) /* 255 */

#endif /* _CSID_H_ */
