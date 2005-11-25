/* -*- Mode: asm; tab-width:4; truncate-lines:t -*-
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

		.file "x86Stub_gas.S"


GLOBAL_ENTRY(staticCompileStub)	

		push	%ebp					/* Make stack frame */
		mov		%esp,%ebp

		/* Even though ESI and EDI are non-volatile (callee-saved) registers, we need to
		   preserve them here in case an exception is thrown.  The exception-handling
		   code expects to encounter this specially-prepared stack "guard" frame when
		   unwinding the stack.  See x86ExceptionHandler.cpp. */
		push	%edi
		push	%esi
		push	%ebx					/* XXX - Why push EBX ?  It's caller-saved */

		/* Call compileAndBackPatchMethod() with 2 args */
		push	16(%esp)				/* Push the second argument to compileAndBackPatchMethod (return address) */
		push	%eax				    /* Push the first argument to compileAndBackPatchMethod (cacheEntry) */
		call	SYMBOL_NAME(compileAndBackPatchMethod)
		
		mov		%ebp,%esp				/* Pop stack frame */
		pop		%ebp
		
		jmp		*%eax					/* Jump to function leaving the return address at the top of the stack */

END_ENTRY(staticCompileStub)	

/*
 * 64bit Arithmetic Support Functions
 *
 * x86Extract64Bit
 *
 * Origin:	Simon
 * Purpose:	signed right-aligned field extraction
 * In:		64 bit source (on  stack)
 *			32 bit extraction size (on stack)
 * Out:		64 bit result
 * Note:	Only works in range 1 <= b <= 63, b is extraction amount
 */
GLOBAL_ENTRY(x86Extract64Bit)

		mov		4(%esp),%eax			/* load low byte of a */

		mov		12(%esp),%ecx			/*load shift amount */
		cmp		$0x20,%ecx
		jg		greater32

		/* extract <= than 32 bits
		 * shift amount = 32 - extract
		 */
		neg		%ecx
		add		$0x20,%ecx				/* ecx = 32 - extract */
		shl		%cl,%eax
		sar		%cl,%eax
		cdq								/* sign extend into EDX:EAX */
		ret		$12
		
greater32:
		/* ext > 32 bits
		 * shift amount = 64 - extract
		 */
		mov		8(%esp),%edx			/* load high byte of a */
		neg		%ecx
		add		$0x40,%ecx				/* ecx = 64 - extract */
		shl		%cl,%edx
		sar		%cl,%edx
		ret		$12

END_ENTRY(x86Extract64Bit)

/*
 * 3WayCompare
 *
 * Origin:	Symantec JIT
 * Purpose:	compare two longs
 * In:		two longs on the stack
 * Out:		depends on condition flags:
 *				less = -1
 *				equal = 0
 *				greater = 1
 */
GLOBAL_ENTRY(x86ThreeWayCMP_L)
		
		/* edx:eax is tos, ecx:ebx is nos */
		mov		8(%esp),%ecx
		mov		16(%esp),%edx

		cmp		%edx,%ecx
		jl		lcmp_m1

		jg		lcmp_1

		mov		4(%esp),%ecx
		mov		12(%esp),%edx

		cmp		%edx,%ecx
		ja		lcmp_1

		mov		$0,%eax
		jb		lcmp_m1

		ret		$16

		.align	4
lcmp_m1:
		mov		$-1,%eax
		ret		$16

		.align	4
lcmp_1:
		mov		$1,%eax
		ret		$16

END_ENTRY(x86ThreeWayCMP_L)

/*
 * llmul
 *
 * Origin:	Intel Code (via MSDev)
 * Purpose: long multiply (same for signed/unsigned)
 * In:		args are passed on the stack:
 *				1st pushed: multiplier (QWORD)
 *				2nd pushed: multiplicand (QWORD)
 * Out:		EDX:EAX - product of multiplier and multiplicand
 * Note:	parameters are removed from the stack
 * Uses:	ECX
 */
GLOBAL_ENTRY(x86Mul64Bit)

		/* A*B = (A.lo * B.lo) + (A.lo * B.hi) + (B.lo * A.hi) ??? */
		mov		8(%esp),%eax			/* A.hi */
		mov		16(%esp),%ecx			/* B.hi */
		or		%eax,%ecx				/* test for both hiwords zero */
		mov		12(%esp),%ecx			/* B.lo */
		jnz		hard	  
		
		/* easy case
		 * both are zero, just mult ALO and BLO
		 */
		mov		4(%esp),%eax			/* A.lo */
		mul		%ecx					/* A.lo * B.lo */
		ret		$16						/* callee restores the stack */

		 /* hard case */
hard:
		push	%ebx

		mul		%ecx					/* A.hi * B.lo */
		mov		%eax,%ebx				/* save result */

		mov		8(%esp),%eax			/* A.lo */
		mull	14(%esp)				/* A.lo * B.hi */
		add		%eax,%ebx				/* ebx = ((A.lo * B.hi) + (A.hi * B.lo)) */

		mov		8(%esp),%eax			/* A.lo */
		mul		%ecx					/* edx:eax = A.lo * B.lo */
		add		%ebx,%edx				/* now edx has all the LO*HI stuff */

		pop		%ebx

		ret		$16						/*  callee restores the stack */

END_ENTRY(x86Mul64Bit)

/*
 * lldiv
 *
 * Origin:	Intel Code (via MSDev)
 * Purpose: signed long divide
 * In:		args are passed on the stack:
 *				1st pushed: divisor (QWORD)
 *				2nd pushed: dividend (QWORD)
 * Out:		EDX:EAX contains the quotient (dividend/divisor)
 * Note:	parameters are removed from the stack
 * Uses:	ECX
 */
GLOBAL_ENTRY(x86Div64Bit)

		push	%edi
		push	%esi
		push	%ebx

		/* Determine sign of the result (%edi = 0 if result is positive, non-zero
		 * otherwise) and make operands positive.
		 */
		xor		%edi,%edi				/* result sign assumed positive */

		mov		20(%esp),%eax			/* hi word of a */
		or		%eax,%eax				/* test to see if signed */
		jge		L1						/* skip rest if a is already positive */
		inc		%edi					/* complement result sign flag */
		mov		16(%esp),%edx			/* lo word of a */
		neg		%eax					/* make a positive */
		neg		%edx
		sbb		$0,%eax
		mov		%eax,20(%esp)			/* save positive value */
		mov		%edx,16(%esp)
L1:
		mov		28(%esp),%eax			/* hi word of b */
		or		%eax,%eax				/* test to see if signed */
		jge		L2						/* skip rest if b is already positive */
		inc		%edi					/* complement the result sign flag */
		mov		24(%esp),%edx			/* lo word of a */
		neg		%eax					/* make b positive */
		neg		%edx
		sbb		$0,%eax
		mov		%eax,28(%esp)			/* save positive value */
		mov		%edx,24(%esp)
L2:

		/* Now do the divide.  First look to see if the divisor is less than 4194304K.
		 * If so, then we can use a simple algorithm with word divides, otherwise
		 * things get a little more complex.
		 * NOTE - %eax currently contains the high order word of DVSR
		 */
		or		%eax,%eax				/* check to see if divisor < 4194304K */
		jnz		L3						/* nope, gotta do this the hard way */
		mov		24(%esp),%ecx			/* load divisor */
		mov		20(%esp),%eax			/* load high word of dividend */
		xor		%edx,%edx
		div		%ecx					/* %eax <- high order bits of quotient */
		mov		%eax,%ebx				/* save high bits of quotient */
		mov		16(%esp),%eax			/* %edx:%eax <- remainder:lo word of dividend */
		div		%ecx					/* %eax <- low order bits of quotient */
		mov		%ebx,%edx				/* %edx:%eax <- quotient */
		jmp		L4						/* set sign, restore stack and return */

		/* Here we do it the hard way.  Remember, %eax contains the high word of DVSR */
L3:
		mov		%eax,%ebx				/* %ebx:ecx <- divisor */
		mov		24(%esp),%ecx
		mov		20(%esp),%edx			/* %edx:%eax <- dividend */
		mov		16(%esp),%eax
L5:
		shr		$1,%ebx					/* shift divisor right one bit */
		rcr		$1,%ecx
		shr		$1,%edx					/* shift dividend right one bit */
		rcr		$1,%eax
		or		%ebx,%ebx
		jnz		L5						/* loop until divisor < 4194304K */
		div		%ecx					/* now divide, ignore remainder */
		mov		%eax,%esi				/* save quotient */

		/* We may be off by one, so to check, we will multiply the quotient
		 * by the divisor and check the result against the original dividend
		 * Note that we must also check for overflow, which can occur if the
		 * dividend is close to 2**64 and the quotient is off by 1.
		 */
		mull	28(%esp)				/* QUOT * HIWORD(DVSR) */
		mov		%eax,%ecx
		mov		24(%esp),%eax
		mul		%esi					/* QUOT * LOWORD(DVSR) */
		add		%ecx,%edx				/* %EDX:%EAX = QUOT * DVSR */
		jc		L6						/* carry means Quotient is off by 1 */

		/* do long compare here between original dividend and the result of the
		 * multiply in %edx:%eax.  If original is larger or equal, we are ok, otherwise
		 * subtract one (1) from the quotient.
		 */
		cmp		20(%esp),%edx			/* compare hi words of result and original */
		ja		L6						/* if result > original, do subtract */
		jb		L7						/* if result < original, we are ok */
		cmp		16(%esp),%eax			/* hi words are equal, compare lo words */
		jbe		L7						/* if less or equal we are ok, else subtract */
L6:
		dec		%esi					/* subtract 1 from quotient */
L7:
		xor		%edx,%edx				/* %edx:%eax <- quotient */
		mov		%esi,%eax

		/* Just the cleanup left to do.  %edx:%eax contains the quotient.  Set the sign
		 * according to the save value, cleanup the stack, and return.
		 */
L4:
		dec		%edi					/* check to see if result is negative */
		jnz		L8						/* if %EDI == 0, result should be negative */
		neg		%edx					/* otherwise, negate the result */
		neg		%eax
		sbb		$0,%edx

		/* Restore the saved registers and return. */
L8:
		pop		%ebx
		pop		%esi
		pop		%edi

		ret		$16

END_ENTRY(x86Div64Bit)

/*
 * llrem
 *
 * Origin:	MSDev
 * Purpose: signed long remainder
 * In:		args are passed on the stack:
 *				1st pushed: divisor (QWORD)
 *				2nd pushed: dividend (QWORD)
 * Out:		%EDX:%EAX contains the quotient (dividend/divisor)
 * Note:	parameters are removed from the stack
 * Uses:	%ECX
 */
GLOBAL_ENTRY(x86Mod64Bit)

		push	%ebx
		push	%edi

		/* Determine sign of the result (%edi = 0 if result is positive, non-zero
		 * otherwise) and make operands positive.
		 */
		xor		%edi,%edi				/* result sign assumed positive */

		mov		16(%esp),%eax			/* hi word of a */
		or		%eax,%eax				/* test to see if signed */
		jge		LL1						/* skip rest if a is already positive */
		inc		%edi					/* complement result sign flag bit */
		mov		12(%esp),%edx			/* lo word of a */
		neg		%eax					/* make a positive */
		neg		%edx
		sbb		$0,%eax
		mov		%eax,16(%esp)			/* save positive value */
		mov		%edx,12(%esp)
LL1:
		mov		24(%esp),%eax			/* hi word of b */
		or		%eax,%eax				/* test to see if signed */
		jge		LL2						/* skip rest if b is already positive */
		mov		20(%esp),%edx			/* lo word of b */
		neg		%eax					/*  make b positive */
		neg		%edx
		sbb		$0,%eax
		mov		%eax,24(%esp)			/* save positive value */
		mov		%edx,20(%esp)
LL2:
		/* Now do the divide.  First look to see if the divisor is less than 4194304K.
		 * If so, then we can use a simple algorithm with word divides, otherwise
		 * things get a little more complex.
		 * NOTE - %eax currently contains the high order word of DVSR
		 */
		or		%eax,%eax				/* check to see if divisor < 4194304K */
		jnz		LL3						/* nope, gotta do this the hard way */
		mov		20(%esp),%ecx			/* load divisor */
		mov		16(%esp),%eax			/* load high word of dividend */
		xor		%edx,%edx
		div		%ecx					/* %edx <- remainder */
		mov		12(%esp),%eax			/* %edx:%eax <- remainder:lo word of dividend */
		div		%ecx					/* %edx <- final remainder */
		mov		%edx,%eax				/* %edx:%eax <- remainder */
		xor		%edx,%edx
		dec		%edi					/* check result sign flag */
		jns		LL4						/* negate result, restore stack and return */
		jmp		LL8						/* result sign ok, restore stack and return */

		/* Here we do it the hard way.  Remember, %eax contains the high word of DVSR */
LL3:
		mov		%eax,%ebx				/* %ebx:%ecx <- divisor */
		mov		20(%esp),%ecx
		mov		16(%esp),%edx			/* %edx:%eax <- dividend */
		mov		12(%esp),%eax
LL5:
		shr		$1,%ebx					/* shift divisor right one bit */
		rcr		$1,%ecx
		shr		$1,%edx					/* shift dividend right one bit */
		rcr		$1,%eax
		or		%ebx,%ebx
		jnz		LL5						/* loop until divisor < 4194304K */
		div		%ecx					/* now divide, ignore remainder */

		/* We may be off by one, so to check, we will multiply the quotient
		 * by the divisor and check the result against the original dividend
		 * Note that we must also check for overflow, which can occur if the
		 * dividend is close to 2**64 and the quotient is off by 1.
		 */
		mov		%eax,%ecx				/* save a copy of quotient in %ECX */
		mull	24(%esp)
		xchg	%eax,%ecx				/* save product, get quotient in %EAX */
		mull	20(%esp)
		add		%ecx,%edx				/* %EDX:%EAX = QUOT * DVSR */
		jc		LL6						/* carry means Quotient is off by 1 */

		/* do long compare here between original dividend and the result of the
		 * multiply in %edx:%eax.  If original is larger or equal, we are ok, otherwise
		 * subtract the original divisor from the result.
		 */
		cmp		16(%esp),%edx			/* compare hi words of result and original */
		ja		LL6						/* if result > original, do subtract */
		jb		LL7						/* if result < original, we are ok */
		cmp		12(%esp),%eax			/* hi words are equal, compare lo words */
		jbe		LL7						/* if less or equal we are ok, else subtract */
LL6:
		sub		20(%esp),%eax			/* subtract divisor from result */
		sbb		24(%esp),%edx
LL7:

		/* Calculate remainder by subtracting the result from the original dividend.
		 * Since the result is already in a register, we will do the subtract in the
		 * opposite direction and negate the result if necessary.
		 */
		sub		12(%esp),%eax			/* subtract dividend from result */
		sbb		16(%esp),%edx

		/* Now check the result sign flag to see if the result is supposed to be positive
		 * or negative.  It is currently negated (because we subtracted in the 'wrong'
		 * direction), so if the sign flag is set we are done, otherwise we must negate
		 * the result to make it positive again.
		 */
		dec		%edi					/* check result sign flag */
		jns		LL8						/* result is ok, restore stack and return */
LL4:
		neg		%edx					/* otherwise, negate the result */
		neg		%eax
		sbb		$0,%edx

		/* Just the cleanup left to do.  %edx:%eax contains the quotient.
		 * Restore the saved registers and return.
		 */
LL8:
		pop		%edi
		pop		%ebx

		ret		$16

END_ENTRY(x86Mod64Bit)

/*
 * llshl
 *
 * Origin:	MSDev. modified
 * Purpose: long shift left
 * In:		args are passed on the stack: (FIX make fastcall) 
 *				1st pushed: amount (int)
 *				2nd pushed: source (long)
 * Out:		%EDX:%EAX contains the result
 * Note:	parameters are removed from the stack
 * Uses:	%ECX, destroyed
 */
GLOBAL_ENTRY(x86Shl64Bit)

		/* prepare from stack */
		mov		4(%esp),%eax
		mov		8(%esp),%edx
		mov		12(%esp),%ecx

		cmp		$64,%cl
		jae		RETZERO

		/* Handle shifts of between 0 and 31 bits */
		cmp		$32,%cl
		jae		MORE32
		shld	%eax,%edx
		shl		%cl,%eax
		ret		$12

		/* Handle shifts of between 32 and 63 bits */
MORE32:
		mov		%eax,%edx
		xor		%eax,%eax
		and		$31,%cl
		shl		%cl,%edx
		ret		$12

		/* return 0 in %edx:%eax */
RETZERO:
		xor		%eax,%eax
		xor		%edx,%edx
		ret		$12

END_ENTRY(x86Shl64Bit)


/*
 * llshr
 *
 * Origin:	MSDev. modified
 * Purpose: long shift right
 * In:		args are passed on the stack: (FIX make fastcall) 
 *				1st pushed: amount (int)
 *				2nd pushed: source (long)
 * Out:		%EDX:%EAX contains the result
 * Note:	parameters are removed from the stack
 * Uses:	%ECX, destroyed
 */
GLOBAL_ENTRY(x86Shr64Bit)

		/* prepare from stack */
		mov		4(%esp),%eax
		mov		8(%esp),%edx
		mov		12(%esp),%ecx

		cmp		$64,%cl
		jae		RRETZERO

		/* Handle shifts of between 0 and 31 bits */
		cmp		$32,%cl
		jae		MMORE32
		shrd	%edx,%eax
		shr		%cl,%edx
		ret		$12

		/* Handle shifts of between 32 and 63 bits */
MMORE32:
		mov		%edx,%eax
		xor		%edx,%edx
		and		$31,%cl
		shr		%cl,%eax
		ret		$12

		/* return 0 in %edx:%eax */
RRETZERO:
		xor		%eax,%eax
		xor		%edx,%edx
		ret		$12

END_ENTRY(x86Shr64Bit)

/*
 * llsar
 *
 * Origin:	MSDev. modified
 * Purpose: long shift right signed
 * In:		args are passed on the stack: (FIX make fastcall) 
 *				1st pushed: amount (int)
 *				2nd pushed: source (long)
 * Out:		%EDX:%EAX contains the result
 * Note:	parameters are removed from the stack
 * Uses:	%ECX, destroyed
 */
GLOBAL_ENTRY(x86Sar64Bit)

		/* prepare from stack */
		mov		4(%esp),%eax
		mov		8(%esp),%edx
		mov		12(%esp),%ecx

		/* Handle shifts of 64 bits or more (if shifting 64 bits or more, the result */
		/* depends only on the high order bit of %edx). */
		cmp		$64,%cl
		jae		RETSIGN

		/* Handle shifts of between 0 and 31 bits */
		cmp		$32,%cl
		jae		MMMORE32
		shrd	%edx,%eax
		sar		%cl,%edx
		ret		$12

		/* Handle shifts of between 32 and 63 bits */
MMMORE32:
		mov		%edx,%eax
		sar		$31,%edx
		and		$31,%cl
		sar		%cl,%eax
		ret		$12

		/* Return double precision 0 or -1, depending on the sign of %edx */
RETSIGN:
		sar		$31,%edx
		mov		%edx,%eax
		ret		$12

END_ENTRY(x86Sar64Bit)
