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

#ifndef nspr_linux_defs_h___
#define nspr_linux_defs_h___

#include <linux/autoconf.h>
#undef	HAVE_LONG_LONG
#undef	HAVE_ALIGNED_DOUBLES
#undef	HAVE_ALIGNED_LONGLONGS

/*
 * ELF linux supports dl* functions.
 * Allow ELF functions for static or modular ELF support.
 */
#if defined(CONFIG_BINFMT_ELF) || defined(CONFIG_BINFMT_ELF_MODULE)
#define	HAVE_DLL
#define	USE_DLFCN
#else
#undef	HAVE_DLL
#undef	USE_DLFCN
#endif

#define NEED_TIME_R

#endif /* nspr_linux_defs_h___ */
