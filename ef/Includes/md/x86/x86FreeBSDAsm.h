/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _MD_X86_FREEBSD_ASM_H_
#define _MD_X86_FREEBSD_ASM_H_

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
#endif /* _MD_X86_FREEBSD_ASM_H_ */
