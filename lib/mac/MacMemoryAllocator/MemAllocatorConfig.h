/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/* must be first in order to correctly pick up TARGET_CARBON define before
 * it is set in ConditionalMacros.h */
#include "DefinesMac.h"

#include <Types.h>
#include <stdlib.h>

#include "IDE_Options.h"

#pragma exceptions off

#ifdef DEBUG

/* Debug macros and switches */

#define DEBUG_HEAP_INTEGRITY	1
#define STATS_MAC_MEMORY		0

#define MEM_ASSERT(condition, message)		((condition) ? ((void)0) : DebugStr("\p"message))


#else

/* Non-debug macros and switches */
#define DEBUG_HEAP_INTEGRITY	0
#define STATS_MAC_MEMORY		0



#define MEM_ASSERT(condition, message)		((void)0)

#endif


