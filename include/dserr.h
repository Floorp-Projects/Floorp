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

#ifndef __DS_ERR_h_
#define __DS_ERR_h_

#define DS_ERROR_BASE				(-0x1000)
#define DS_ERROR_LIMIT				(DS_ERROR_BASE + 1000)

#define IS_DS_ERROR(code) \
    (((code) >= DS_ERROR_BASE) && ((code) < DS_ERROR_LIMIT))

/*
** DS library error codes
*/
#define DS_ERROR_OUT_OF_MEMORY			(DS_ERROR_BASE + 0)

#endif /* __DS_ERR_h_ */
