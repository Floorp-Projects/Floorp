/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#ifndef _XP_Assert_
#define _XP_Assert_

#include "xp_trace.h"
/*include <stdlib.h>*/

/*-----------------------------------------------------------------------------
	abort
	
	For debug builds...
	XP_ABORT(X), unlike abort(), takes a text string argument. It will print
	it out and then call abort (to drop you into your debugger).
	
	For release builds...
	XP_ABORT will call abort(). Whether you #define NDEBUG or not is up
	to you.
-----------------------------------------------------------------------------*/
#define XP_ABORT(MESSAGE)	(XP_TRACE(MESSAGE),abort())

/*-----------------------------------------------------------------------------
	XP_ASSERT is just like "assert" but calls XP_ABORT if it fails the test.
	I need this (on a Mac) because "assert" and "abort" are braindead,
	whereas my XP_Abort function will invoke the debugger. It could
	possibly have been easier to just #define assert to be something decent.
-----------------------------------------------------------------------------*/

#if defined (XP_UNIX)
#if !defined(NO_UNIX_SKIP_ASSERTS)
/* Turning UNIX_SKIP_ASSERTS on by default. */
/* (Solaris 2.x) on Sol2.5, assert() does not work. Too bad...  */
/* Therefore, we print the line where assert happened instead. */
/* Print out a \007 to sound the bell. -mcafee */
#define XP_AssertAtLine() fprintf(stderr, "assert: line %d, file %s%c\n", __LINE__, __FILE__, 7)
#ifdef DEBUG
#define XP_ASSERT(X) ( (((X))!=0)? (void)0: (void)XP_AssertAtLine() )
#else
#define XP_ASSERT(X) (void)0
#endif
#else
#include <assert.h>
#define XP_ASSERT(X)			assert(X) /* are we having fun yet? */
#endif

#elif defined (XP_BEOS)

#ifdef DEBUG
#include <assert.h>
#define XP_ASSERT(X)            assert(X)
#else
#define XP_ASSERT(X)
#endif

#elif defined (XP_WIN)
#ifdef DEBUG

/* LTNOTE: I got tired of seeing all asserts at FEGUI.CPP.  This should
 * Fix the problem.  I intentionally left out Win16 because strings are stuffed
 * into the datasegment we probably couldn't build.
*/
#ifdef WIN32
XP_BEGIN_PROTOS
extern void XP_AssertAtLine( char *pFileName, int iLine );
XP_END_PROTOS
#define XP_ASSERT(X)   ( ((X)!=0)? (void)0: XP_AssertAtLine(__FILE__,__LINE__))

#else  /* win16 */
#define XP_ASSERT(X)                ( ((X)!=0)? (void)0: XP_Assert((X) != 0)  )
XP_BEGIN_PROTOS
void XP_Assert(int);
XP_END_PROTOS
#endif

#else
#define XP_ASSERT(X)  ((void) 0)
#endif 

#elif defined (XP_OS2)
  #ifdef DEBUG
    #include <assert.h>
    #define XP_ASSERT(X)            assert(X) /* IBM-DAK same as UNIX */
  #else
    #define XP_ASSERT(X)
  #endif

#elif defined(XP_MAC)

	#ifdef DEBUG
		#ifndef XP_ASSERT
			#include <Memory.h>
			#include <string.h>
			/* Carbon doesn't support debugstr(), so we have to do it ourselves. Also, Carbon */
			/* may have read-only strings so that we need a temp buffer to use c2pstr(). */
			static StringPtr XP_c2pstrcpy(StringPtr pstr, const char* str)
			{
				int len = (int) strlen(str);
				if (len > 255) len = 255;
				pstr[0] = (unsigned char)len;
				BlockMoveData(str, pstr + 1, len);
				return pstr;
			}
			#define XP_ASSERT(X) do {if (!(X)) {Str255 pstr; DebugStr(XP_c2pstrcpy(pstr, #X));} } while (PR_FALSE)
		#endif
	#else
		#define XP_ASSERT(X)
	#endif

#endif /* XP_MAC */

/*-----------------------------------------------------------------------------
	assert variants
	
	XP_WARN_ASSERT if defined to nothing for release builds. This means
	that instead of
				#ifdef DEBUG
				assert (X);
				#endif
	you can just do
				XP_WARN_ASSERT(X);
	
	Of course when asserts fail that means something is going wrong and you
	*should* have normal code to deal with that.
	I frequently found myself writing code like this:
				#ifdef DEBUG
				assert (aPtr);
				#endif
				if (!aPtr)
					return error_something_has_gone_wrong;
	so I just combined them into a macro that can be used like this:
				if (XP_FAIL_ASSERT(aPtr))
					return; // or whatever else you do when things go wrong
	What this means is it will return if X *fails* the test. Essentially
	the XP_FAIL_ASSERT bit replaces the "!" in the if test.
	
	XP_OK_ASSERT is the opposite. If if you want to do something only if
	something is OK, then use it. For example:
				if (XP_OK_ASSERT(aPtr))
					aPtr->aField = 25;
	Use this if you are 99% sure that aPtr will be valid. If it ever is not,
	you'll drop into the debugger. For release builds, it turns into an
	if statement, so it's completely safe to execute.

        You can also do XP_VERIFY, which essentially will throw an assert if a
        condition fails in debug mode, but just do whatever at runtime. For
        example:

                XP_VERIFY(PR_LoadLibrary("foo") == 0);

        This will trigger an XP_ASSERT if the condition fails during debug, bug
        just run the PR_LoadLibrary in release. Kind of the same as XP_WARN_ASSERT,
        but the "verbiage" is a bit clearer (to me, anyway).
-----------------------------------------------------------------------------*/
#ifdef DEBUG
#	define XP_WARN_ASSERT(X)	(  ((X)!=0)? (void)0: XP_ABORT((#X))  )
#	define XP_OK_ASSERT(X)		(((X)!=0)? 1: (XP_ABORT((#X)),0))
#	define XP_FAIL_ASSERT(X)	(((X)!=0)? 0: (XP_ABORT((#X)),1))
#       define XP_VERIFY(X)             (  (X)? (void)0: XP_ASSERT(0)  )
#else
#	define XP_WARN_ASSERT(X)	(void)((X)!=0)
#	define XP_OK_ASSERT(X)		(((X)!=0)? 1: 0)
#	define XP_FAIL_ASSERT(X)	(((X)!=0)? 0: 1)
#       define XP_VERIFY(X)             (  (void)(X)  )
#endif

#endif /* _XP_Assert_ */

