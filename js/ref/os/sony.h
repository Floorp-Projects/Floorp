/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nspr_sony_defs_h___
#define nspr_sony_defs_h___
 
#undef	HAVE_STACK_GROWING_UP
#undef	HAVE_LONG_LONG
#undef	HAVE_ALIGNED_DOUBLES
#undef	HAVE_ALIGNED_LONGLONGS
#define	HAVE_DLL
#define	USE_DLFCN
#define	NEED_TIME_R
 
/*
** Missing function prototypes
*/
extern int gethostname(char *name, int namelen);
 
#endif /* nspr_sony_defs_h___ */
