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
#define OLDROUTINELOCATIONS 0

#define XP_MAC 1
#define HAVE_LONG_LONG
#define MISALIGNED_MEMORY_ACCESS_OK
#define _PR_NO_PREEMPT 1
#define _PR_USECPU 1
#define _NO_FAST_STRING_INLINES_ 1
#ifndef DEBUG
 #define NDEBUG 1
#endif

// DEBUG_LOG controls whether we compile in the routines to disassemble and dump
// internal compiler state to a file or stdout.  These are useful even in nondebug
// builds but should not be present in release builds.
// DEBUG_LOG is equivalent to PR_LOGGING; perhaps we should merge these two?
#ifndef RELEASE
 #define FORCE_PR_LOG 1
 #define DEBUG_LOG
#endif

#include "IDE_Options.h"

#ifndef DEBUG
  // Faithfully obey our requests for inlining functions.  Without this Metrowerks
  // will not inline functions more than four levels deep, which is a serious problem
  // for class hierarchies with more than four levels of simple inline constructors.
  #pragma always_inline on
#endif
