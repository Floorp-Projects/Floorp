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
