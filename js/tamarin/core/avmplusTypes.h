/* ***** BEGIN LICENSE BLOCK ***** 
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1 
 *
 * The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
 * "License"); you may not use this file except in compliance with the License. You may obtain 
 * a copy of the License at http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
 * WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
 * language governing rights and limitations under the License. 
 * 
 * The Original Code is [Open Source Virtual Machine.] 
 * 
 * The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
 * by the Initial Developer are Copyright (C)[ 2004-2006 ] Adobe Systems Incorporated. All Rights 
 * Reserved. 
 * 
 * Contributor(s): Adobe AS3 Team
 * 
 * Alternatively, the contents of this file may be used under the terms of either the GNU 
 * General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
 * License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
 * LGPL are applicable instead of those above. If you wish to allow use of your version of this 
 * file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
 * version of this file under the terms of the MPL, indicate your decision by deleting provisions 
 * above and replace them with the notice and other provisions required by the GPL or the 
 * LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
 * under the terms of any one of the MPL, the GPL or the LGPL. 
 * 
 ***** END LICENSE BLOCK ***** */

#ifndef __avmplus_types__
#define __avmplus_types__


#ifdef _MAC
#include <stdint.h>
#endif

//#define _BigEndian

namespace avmplus
{
	typedef unsigned char    byte;
	typedef unsigned char    uint8;
	typedef unsigned short   uint16;
	typedef char             sint8;
	typedef char             int8;
	typedef short            sint16;
	typedef short            int16;
	typedef unsigned int     uint32; 
	typedef signed int       sint32;
	typedef signed int       int32;
	typedef unsigned long	 intptr;
	#ifdef _MSC_VER
	typedef __int64          int64;
	typedef __int64          sint64;
	typedef unsigned __int64 uint64;
	#elif defined(_MAC)
	typedef int64_t          int64;
	typedef int64_t          sint64;
	typedef uint64_t         uint64;
	#else
	typedef long long          int64;
	typedef long long          sint64;
	typedef unsigned long long         uint64;
	#endif

	/* wchar is our version of wchar_t, since wchar_t is different sizes
	   on different platforms, but we want to use UTF-16 uniformly. */
	typedef unsigned short wchar;
	
    #ifndef NULL
    #define NULL 0
    #endif

	typedef signed long          Atom;
	typedef signed long			 Binding;
	typedef signed long          CodeContextAtom;

	inline uint32 urshift(Atom atom, int amount)
	{
		return ((uint32)atom >> amount);
	}
}

#endif /* __avmplus_types__ */
