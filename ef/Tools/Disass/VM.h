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
#ifndef _VM_H_
#define _VM_H_

typedef unsigned int Uint32;
typedef signed int Int32;
typedef unsigned char Uint8;
typedef signed char Int8;

typedef struct MethodBlock_t
{
	Uint32 pad[6];
	Uint8* code;
} MethodBlock;

typedef struct CodeCacheEntry_t 
{
	Uint32 pad1[2];
	Uint32 length;
	Uint32 pad2[2];
	MethodBlock* methodBlock;
} CodeCacheEntry;

const Uint32 CALL_OPCODE   = 0xE8;
const Uint32 RETURN_OPCODE = 0xC3;

#endif /* _VM_H_ */
