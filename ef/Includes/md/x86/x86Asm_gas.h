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

#ifndef _MD_X86ASM_GAS_H_
#define _MD_X86ASM_GAS_H_

#define SYMBOL_NAME_STR(name) #name
#define SYMBOL_NAME(name) name
#define SYMBOL_NAME_LABEL(name) CAT(name,:)

#if !defined(__i486__) && !defined(__i586__)
#define ALIGN .align 4,0x90
#define ALIGN_STR ".align 4,0x90"
#else  /* __i486__/__i586__ */
#define ALIGN .align 16,0x90
#define ALIGN_STR ".align 16,0x90"
#endif /* __i486__/__i586__ */

#define STATIC_ENTRY(name)				\
	ALIGN;								\
	SYMBOL_NAME_LABEL(name)

#define GLOBAL_ENTRY(name)				\
	.globl SYMBOL_NAME(name);			\
	STATIC_ENTRY(name)

#define END_ENTRY(name)					\
	SYMBOL_NAME_LABEL(CAT(.L,name));	\
	.size SYMBOL_NAME(name),SYMBOL_NAME(CAT(.L,name))-SYMBOL_NAME(name)

#define TEXT_SEGMENT .text
#define DATA_SEGMENT .data
#endif /* _MD_X86ASM_GAS_H_ */
