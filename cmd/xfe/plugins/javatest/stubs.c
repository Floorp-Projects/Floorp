/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
