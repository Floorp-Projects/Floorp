/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "prosdep.h"	/* include first to get the right #defines */
#include "../common/npunix.c"
#include "_stubs/JavaTestPlugin.c"
#include "_stubs/java_lang_String.c"
#ifdef EMBEDDED_FRAMES
#include "_stubs/java_awt_Color.c"
#include "_stubs/java_awt_EmbeddedFrame.c"
#endif

/*
** !!! Edit the makefiles to add a dependency on any .c file included here. 
** Keep them up to date. !!!
*/
