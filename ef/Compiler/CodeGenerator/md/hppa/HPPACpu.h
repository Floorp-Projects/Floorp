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

#ifndef _HPPA_CPU_H_
#define _HPPA_CPU_H_

#define FIRST_CALLER_SAVED_GR       0
#define LAST_CALLER_SAVED_GR        12
#define FIRST_CALLEE_SAVED_GR       13
#define LAST_CALLEE_SAVED_GR        28
#define FIRST_CALLER_SAVED_FPR      29
#define LAST_CALLER_SAVED_FPR       56
#define FIRST_CALLEE_SAVED_FPR      57
#define LAST_CALLEE_SAVED_FPR       76

#define FIRST_GREGISTER             0
#define LAST_GREGISTER              28
#define FIRST_FPREGISTER            29
#define LAST_FPREGISTER             76
#define NUMBER_OF_SPECIAL_REGISTERS 3

#define HAS_GR_PAIRS
#define HAS_FPR_PAIRS

#define DOUBLE_WORD_GR_ALIGNMENT

#undef SOFT_FLOAT

#undef  SINGLE_WORD_FPR_ALIGNMENT
#define SINGLE_WORD_FPR_SIZE     1

#define DOUBLE_WORD_FPR_ALIGNMENT
#define DOUBLE_WORD_FPR_SIZE     2

#define QUAD_WORD_FPR_ALIGNMENT  
#define QUAD_WORD_FPR_SIZE       4

#define CPU_IS_SUPPORTED

#if defined(INCLUDE_EMITTER)
#include "HPPAEmitter.h"
typedef HPPAEmitter            MdEmitter;
#endif /* INCLUDE_EMITTER */

#endif /* _HPPA_CPU_H_ */
