/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/*
 * PR assertion checker.
 */

#ifndef jsutil_h___
#define jsutil_h___

JS_BEGIN_EXTERN_C

/***********************************************************************
** FUNCTION:	JS_MALLOC()
** DESCRIPTION:
**   JS_NEW() allocates an untyped item of size _size from the heap.
** INPUTS:  _size: size in bytes of item to be allocated
** OUTPUTS:	untyped pointer to the node allocated
** RETURN:	pointer to node or error returned from malloc().
***********************************************************************/
#define JS_MALLOC(_bytes) (malloc((_bytes)))

/***********************************************************************
** FUNCTION:	JS_DELETE()
** DESCRIPTION:
**   JS_DELETE() unallocates an object previosly allocated via JS_NEW()
**   or JS_NEWZAP() to the heap.
** INPUTS:	pointer to previously allocated object
** OUTPUTS:	the referenced object is returned to the heap
** RETURN:	void
***********************************************************************/
#define JS_DELETE(_ptr) { free(_ptr); (_ptr) = NULL; }

/***********************************************************************
** FUNCTION:	JS_NEW()
** DESCRIPTION:
**   JS_NEW() allocates an item of type _struct from the heap.
** INPUTS:  _struct: a data type
** OUTPUTS:	pointer to _struct
** RETURN:	pointer to _struct or error returns from malloc().
***********************************************************************/
#define JS_NEW(_struct) ((_struct *) JS_MALLOC(sizeof(_struct)))

#ifdef DEBUG

extern JS_PUBLIC_API(void)
JS_Assert(const char *s, const char *file, JSIntn ln);
#define JS_ASSERT(_expr) \
    ((_expr)?((void)0):JS_Assert(# _expr,__FILE__,__LINE__))

#define JS_NOT_REACHED(_reasonStr) \
    JS_Assert(_reasonStr,__FILE__,__LINE__)

#else

#define JS_ASSERT(expr) ((void) 0)
#define JS_NOT_REACHED(reasonStr)

#endif /* defined(DEBUG) */

/*
** Abort the process in a non-graceful manner. This will cause a core file,
** call to the debugger or other moral equivalent as well as causing the
** entire process to stop.
*/
extern JS_PUBLIC_API(void) JS_Abort(void);

JS_END_EXTERN_C

#endif /* jsutil_h___ */
