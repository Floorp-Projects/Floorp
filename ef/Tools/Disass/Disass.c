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
#include <windows.h>
#include <native.h>
#include <stdio.h>
#include <assert.h>
#include "Disass.h"
#include "VM.h"
#include "XDisAsm.h"

Uint32 knownStub[] = 
{
	0x55076A50, 0x04246C8D, 0x6A535756, 0x73E85500,
	0x89FFFFFB, 0x64500445, 0x000035FF, 0xC7640000,
	0x00000005, 0xFFFFFF00, 0x0875FFFF, 0x000023E8,
	0x058F6400, 0x00000000, 0x840FC00B, 0x0002C832,
	0x3FE8F08B, 0x8BFFFFFB, 0x5F5B5BC6, 0x59595D5E,
	0x1860FF58, 0x57565553
};
const Uint32 knownStubSize = sizeof(knownStub);

DWORD __cdecl 
RNIGetCompatibleVersion()
{
	return RNIVER;
}

static long printMethod(struct HDisass *pThis, MethodBlock* method)
{
	Uint8* code;
	Uint8* limit;
	Uint8* start;
	CodeCacheEntry* codeCache;

	code = method->code;

	// If the code is not yet compiled then the first instruction
	// to be executed will be a call to the compile stub.
	if (code[0] == CALL_OPCODE) {
		Int32 offset;
		Uint8* staticCompileStub;
		Uint8* newCompileStub;

		staticCompileStub = &code[5] + *(Int32 *) &code[1];
	
		if (memcmp(staticCompileStub, knownStub, knownStubSize) != 0) {
			fprintf(stderr, "Unknown stub for this method");
			return Disass_UNKNOWN_STUB;
		}

		newCompileStub = (Uint8 *) malloc(knownStubSize);
		memcpy(newCompileStub, staticCompileStub, knownStubSize);

		// Replace the call offsets.
		offset = *(Int32 *) &staticCompileStub[15];
		*(Int32 *) &newCompileStub[15] = offset + (staticCompileStub - newCompileStub);

		offset = *(Int32 *) &staticCompileStub[45];
		*(Int32 *) &newCompileStub[45] = offset + (staticCompileStub - newCompileStub);

		offset = *(Int32 *) &staticCompileStub[67];
		*(Int32 *) &newCompileStub[67] = offset + (staticCompileStub - newCompileStub);

		// Put a return at the end of the code instead of the jump code.
		newCompileStub[81] = RETURN_OPCODE;

		_asm {
			call stub
			jmp end
	stub:
			push method
			jmp newCompileStub
	end:
		}

		code = method->code;
	}

	codeCache = (CodeCacheEntry *) (code - sizeof(CodeCacheEntry));
	limit = ((Uint8 *) codeCache) + codeCache->length + 4;

	start = code;
	while ((code < limit) && (0 != *(Uint32 *) code)) {
		fprintf(stdout, "%p:\t", code - start);
		fprintf(stdout, "%s\n", disasmx86(code - start, &code, limit + 1, kDisAsmFlag32BitSegments));
	}

	return Disass_SUCCESS;
}

__declspec(dllexport) long __cdecl 
Disass_disassemble(struct HDisass *pThis, struct Hjava_lang_String *jClassName, struct Hjava_lang_String *jMethodName,
				   struct Hjava_lang_String *jSignature)
{
	ClassClass* classclass;
	MethodBlock* method;

	char cClassName[1024];
	char cMethodName[1024];
	char cSignature[1024];

	javaString2CString(jClassName, cClassName, 1024);
	javaString2CString(jMethodName, cMethodName, 1024);
	javaString2CString(jSignature, cSignature, 1024);

	classclass = FindClassEx(cClassName, FINDCLASSEX_IGNORECASE);
	if (classclass == NULL)
		return Disass_CLASS_NOT_FOUND;

	method = (MethodBlock*) Class_GetMethod(classclass, cMethodName, cSignature);
	if (method == NULL)
		return Disass_METHOD_NOT_FOUND;

	return printMethod(pThis, method);
}

