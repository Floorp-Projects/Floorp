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

#ifndef __XP_ERROR_h_
#define __XP_ERROR_h_


#include "xp_core.h"
#include <errno.h>


XP_BEGIN_PROTOS

extern int xp_errno;

/*
** Return the most recent set error code.
*/
#ifdef XP_WIN
#define XP_GetError() xp_errno
#else
#define XP_GetError() xp_errno
#endif

/*
** Set the error code
*/
#ifdef DEBUG
extern void XP_SetError(int value);
#else
#define XP_SetError(v) xp_errno = (v)
#endif


XP_END_PROTOS


#endif /* __XP_ERROR_h_ */
