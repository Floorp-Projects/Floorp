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

#ifndef nspr_irix_defs_h___
#define nspr_irix_defs_h___

#define	HAVE_LONG_LONG
#ifdef IRIX6_2
#define	HAVE_ALIGNED_DOUBLES
#else
#undef	HAVE_ALIGNED_DOUBLES
#endif /* IRIX6_2 */
#undef	HAVE_ALIGNED_LONGLONGS
#define	HAVE_WEAK_IO_SYMBOLS
#define	HAVE_WEAK_MALLOC_SYMBOLS
#define	HAVE_DLL
#define	USE_DLFCN

#endif /* nspr_irix_defs_h___ */
