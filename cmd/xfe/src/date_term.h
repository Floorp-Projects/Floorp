/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/* -*- Mode: C++; tab-width: 4 -*-
   date_term.h -- the common headers for mail filter and messsage search 
		   date terms
   Created: Seth Spitzer <sspitzer@netscape.com>, 06-15-1998
 */

#ifndef _xfe_date_term_h_
#define _xfe_date_term_h_

extern int XFE_SEARCH_INVALID_DATE;

/* sspitzer:
 * we aren't using these yet.  But we will eventually.
extern int XFE_SEARCH_INVALID_MONTH;
extern int XFE_SEARCH_INVALID_DAY;
extern int XFE_SEARCH_INVALID_YEAR;
 */

/* sspitzer:
 * part of fix for bug: #138740
 *
 * MAX_BYTES_NEEDED_FOR_XP_SEARCH_DATE_FORMAT needs to be >= the
 * maximum size strftime(x,y,XP_GetString(XFE_SEARCH_DATE_FORMAT), z)
 * could return.   for "%m/%d/%Y", example: "01/01/1998"
 * which needs 11 bytes, strlen("01/01/1998") + one for '\0'
 *
 * so if you change XFE_SEARCH_DATE_FORMAT (in ns/cmd/xfe/xfe_err.h)
 * you better change MAX_BYTES_NEEDED_FOR_XP_SEARCH_DATE_FORMAT
 */
#define MAX_BYTES_NEEDED_FOR_XP_SEARCH_DATE_FORMAT 40

#endif /* _xfe_date_term_h_ */
