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

#include <md/Asm.h>

GLOBAL_ENTRY(sysThrow)
		push %ebp
		mov	%esp,%ebp
		push 8(%ebp)
		push %ebx
		push %esi
		push %edi
		push %ebp

		call SYMBOL_NAME(x86SoftwareExceptionHandler)

/**
	restore when regalloc is fixed

GLOBAL_ENTRY(x86JumpToHandler)
		popl %eax
		popl %eax
		popl %ecx
		popl %ebx
		popl %esi
		popl %edi
		popl %ebp
		popl %esp
		jmp *%ecx
		
		// FIX (hack to work around reg alloc bug)
**/
GLOBAL_ENTRY(x86JumpToHandler)
		popl %eax
		popl %ecx
		popl %eax
		popl %ebx
		popl %esi
		popl %edi
		popl %ebp
		popl %esp
		jmp *%eax	

GLOBAL_ENTRY(sysThrowClassCastException)
		mov $SYMBOL_NAME(exceptionClass), %eax
		push 8(%eax)
		call SYMBOL_NAME(x86NewExceptionInstance)
		popl %ecx
		pushl %eax
		pushl %ecx
		jmp SYMBOL_NAME(sysThrow);

GLOBAL_ENTRY(sysThrowNullPointerException)
		mov $SYMBOL_NAME(exceptionClass), %eax
		push (%eax)
		call SYMBOL_NAME(x86NewExceptionInstance)
		popl %ecx
		pushl %eax
		pushl %ecx
		jmp SYMBOL_NAME(sysThrow);

GLOBAL_ENTRY(sysThrowArrayIndexOutOfBoundsException)
		mov $SYMBOL_NAME(exceptionClass), %eax
		push 4(%eax)
		call SYMBOL_NAME(x86NewExceptionInstance)
		popl %ecx
		pushl %eax
		pushl %ecx
		jmp SYMBOL_NAME(sysThrow);
		
GLOBAL_ENTRY(sysThrowArrayStoreException)
		mov $SYMBOL_NAME(exceptionClass), %eax
		push 12(%eax)
		call SYMBOL_NAME(x86NewExceptionInstance)
		popl %ecx
		pushl %eax
		pushl %ecx
		jmp SYMBOL_NAME(sysThrow);
