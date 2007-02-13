/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "avmplus.h"
#include <setjmp.h>


extern "C"
{
	__declspec(naked)
	int __cdecl _setjmp3(jmp_buf /*jmpbuf*/, int /*arg*/)
	{
		// warning: the order of the registers in jmpbuf is used in
		// BufferGuard for in memory swf buffer protection.  See
		// GrowableBuffer.cpp
		_asm
		{
			mov edx, [esp+4]
			mov DWORD PTR [edx],ebp
			mov DWORD PTR [edx+4],ebx
			mov DWORD PTR [edx+8],edi
			mov DWORD PTR [edx+12],esi
			mov DWORD PTR [edx+16],esp
			mov eax, DWORD PTR [esp]
			mov DWORD PTR [edx+20],eax
			mov DWORD PTR [edx+24],0xFFFFFFFF
			mov DWORD PTR [edx+28],0xFFFFFFFF
			mov DWORD PTR [edx+32],0x56433230
			mov DWORD PTR [edx+36],0
			sub eax,eax
			ret
		}
	}

	// Disable the "ebp was modified" warning.
	// We really do want to modify it.
    #pragma warning ( disable : 4731 )

	__declspec(noreturn)
	void __cdecl longjmp(jmp_buf jmpbuf, int result)
	{
		_asm
		{
			mov edx,[jmpbuf]
			mov eax,[result]
			mov ebp,DWORD PTR [edx]
			mov ebx,DWORD PTR [edx+4]
			mov edi,DWORD PTR [edx+8]
			mov esi,DWORD PTR [edx+12]
			mov esp,DWORD PTR [edx+16]
			add esp,4
			jmp DWORD PTR [edx+20]
		}
	}
}
