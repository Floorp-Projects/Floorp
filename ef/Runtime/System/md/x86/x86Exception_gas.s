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

		.globl sysThrow
		.align 4
sysThrow:
		push %ebp
		mov	%esp,%ebp
		push 8(%ebp)
		push %ebx
		push %esi
		push %edi
		push %ebp

		call x86SoftwareExceptionHandler

/**
	restore when regalloc is fixed

		.globl x86JumpToHandler
		.align 4
x86JumpToHandler:
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
		.globl x86JumpToHandler
		.align 4
x86JumpToHandler:
		popl %eax
		popl %ecx
		popl %eax
		popl %ebx
		popl %esi
		popl %edi
		popl %ebp
		popl %esp
		jmp *%eax	

		.globl sysThrowClassCastException
		.align 4
sysThrowClassCastException:
		mov $exceptionClass, %eax
		push 8(%eax)
		call x86NewExceptionInstance
		popl %ecx
		pushl %eax
		pushl %ecx
		jmp sysThrow;

		.globl sysThrowNullPointerException
		.align 4
sysThrowNullPointerException:
		mov $exceptionClass, %eax
		push (%eax)
		call x86NewExceptionInstance
		popl %ecx
		pushl %eax
		pushl %ecx
		jmp sysThrow;

		.globl sysThrowArrayIndexOutOfBoundsException
		.align 4
sysThrowArrayIndexOutOfBoundsException:
		mov $exceptionClass, %eax
		push 4(%eax)
		call x86NewExceptionInstance
		popl %ecx
		pushl %eax
		pushl %ecx
		jmp sysThrow;
		
		.globl sysThrowArrayStoreException
		.align 4
sysThrowArrayStoreException:
		mov $exceptionClass, %eax
		push 12(%eax)
		call x86NewExceptionInstance
		popl %ecx
		pushl %eax
		pushl %ecx
		jmp sysThrow;
