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

#ifndef __CAST_H
#define __CAST_H

//	To handle casts between the various windows
//		platforms.
//	We use these macros/inline functions instead of just
//		casting directly so that we can manipulate their
//		behaviour on a broad scale, and very customized
//		behaviour can be achieved if you need to range
//		check values in debug mode, etc.

//	PAY ATTENTION, OR WE'RE ALL DEAD:
//	The use of inline functions is to avoid the over evaluation
//		problem (common to macros) when repeating parameters
//		in doing a computation.
//		i.e.
//			#define EVAL(a) ((a) + (a))
//
//			EVAL(a *= a + a);
//		However, by using inline function, it is not possible
//		to provide every possible cast operation that macros
//		do cover.  This is a known shortfall.  Templates were
//		the solution, but it's not clear that we'll ever have
//		a 16 bit compiler which supports templates.  You will
//		have to implement each needed cast seperately.  You
//		can mostly avoid the problem by using the largest possible
//		data type.

//	To turn off all casting in the front end which
//		utilizes these macros, define NO_CAST, and get
//		ready for a boatload of warnings.
//#define NO_CAST


//	Now onto the show.
//	GAB 09-13-95


//	Native integer cast, good for use in WinAPI calls.
//	In debug, we camp on you if we actually lose bits, as it
//		is ambiguous if we should hold the top limit to MAXINT
//		or if we should just take the lower sizeof(int) bytes
//		and ignore the rest.
#ifndef NO_CAST
#ifndef _DEBUG
#define CASTINT(a) ((int)(a))
#else
inline int CASTINT(long a)	{
	int b = (int)a;

	ASSERT(a == (long)b);
	return(b);
}
#endif
#else
#define CASTINT(a) (a)
#endif

//	Native unsigned cast.
//	In debug, we camp on you if we actually lose bits.
#ifndef NO_CAST
#ifndef _DEBUG
#define CASTUINT(a) ((unsigned)(a))
#else
inline unsigned CASTUINT(long a)	{
	unsigned b = (unsigned)a;

	ASSERT(a == (long)b);
	return(b);
}
#endif
#else
#define CASTUINT(a) (a)
#endif

#ifndef NO_CAST
#ifndef _DEBUG
#define CASTSIZE_T(a) ((size_t)(a))
#else
inline size_t CASTSIZE_T(long a)	{
	size_t b = (size_t)a;

	ASSERT(a == (long)b);
	return(b);
}
#endif
#else
#define CASTSIZE_T(a) (a)
#endif

#ifndef NO_CAST
#ifndef _DEBUG
#define CASTDWORD(a) ((DWORD)(a))
#else
inline DWORD CASTDWORD(long a)	{
	DWORD b = (DWORD)a;

	ASSERT(a == (long)b);
	return(b);
}
#endif
#else
#define CASTDWORD(a) (a)
#endif

//	NCAPI data from URL_Struct
#define NCAPIDATA(pUrl) ((CNcapiUrlData *)(pUrl)->ncapi_data)

//  Context casts
#define CX2VOID(pContext, CastFromClassCX) ((void *)((CAbstractCX *)((CastFromClassCX *)(pContext))))
#define VOID2CX(pVoid, CastToClassCX) ((CastToClassCX *)((CAbstractCX *)((void *)(pVoid))))

#define ABSTRACTCX(pXPCX) ((pXPCX)->fe.cx)
#define BOOKMARKCX(pXPCX) ((CNewBookmarkWnd *)((pXPCX)->fe.cx))
#define WINCX(pXPCX) ((CWinCX *)((pXPCX)->fe.cx))
#define CXDC(pXPCX) ((CDCCX *)((pXPCX)->fe.cx))
#define PANECX(pXPCX) ((CPaneCX *)((pXPCX)->fe.cx))


#endif // __CAST_H
