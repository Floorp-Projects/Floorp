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
 * Portions created by the Initial Developer are Copyright (C) 1993-2006
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


#ifndef __AvmDebug__
#define __AvmDebug__

#include "avmplusTypes.h"

#ifdef WIN32
#include <windows.h>
#endif

#ifdef _MAC
#if !TARGET_RT_MAC_MACHO
typedef const unsigned char* ConstStr255Param;
extern "C"
{
#ifdef powerc
	extern pascal void DebugStr(ConstStr255Param aStr);
#else
	// extern pascal void DebugStr(ConstStr255Param aStr) = 0xABFF;
	extern pascal void SysBreakStr(ConstStr255Param aStr) = {0x303C, 0xFE15, 0xA9C9};
#endif
}
#endif
#endif

namespace avmplus
{
	void AvmDebugMsg(bool debuggerBreak, const char* format, ...);
	void AvmDebugMsg(const char* msg, bool debuggerBreak);
	void AvmDebugMsg(const wchar* msg, bool debuggerBreak);

	#ifdef _DEBUG
		inline void _AvmAssertMsg(int32 assertion, const char* msg)
		{
			if (assertion == 0)
				AvmDebugMsg(msg, true);
		}

		#define AvmAssertMsg(x,y)				do { _AvmAssertMsg((x), (y)); } while (0) /* no semi */

		#define AvmAssert(x)					_AvmAssert((x), __LINE__,__FILE__)
		#define _AvmAssert(x, line_, file_)		__AvmAssert((x), line_, file_)
		#define __AvmAssert(x, line_, file_)	do { AvmAssertMsg((x), "Assertion failed: \"" #x "\" (" #file_ ":" #line_ ")"); } while (0) /* no semi */
		
	#else
		#define AvmAssertMsg(x,y)	do { } while (0) /* no semi */
		#define AvmAssert(x)		do { } while (0) /* no semi */
	#endif

	/*************************************************************************/
	/******************************* Debugging *******************************/
	/*************************************************************************/

	/* This mess serves to define the DebugMsg function on each platform.
	* DebugMsg is only defined when the Debug flag is turned on; it halts
	* program execution and drops into the debugger with the given message.
	* We define it as in inline so that when you fall into the debugger,
	* you're in the function that issued the call and not in a "DebugMsg"
	* subroutine.
	*/
	#if defined(_MAC) && !defined(DARWIN)
		// WARNING: this function is NOT THREAD SAFE
	    /*plugin_export*/ ConstStr255Param MakePascalMsg(const char* theString);
		
		#if SOFT_ASSERTS
			inline void DebugMsg_(const char* msg) { }
		#else
			#ifdef powerc
				inline void DebugMsg_(const char* msg) { DebugStr(MakePascalMsg(msg)); }
			#else
				inline void DebugMsg_(const char* msg) { SysBreakStr(MakePascalMsg(msg)); }
			#endif
		#endif // SOFT_ASSERTS
	#endif
}

#endif /* __AvmDebug__ */
