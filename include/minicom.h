/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */


/*******************************************************************************
 * Mini Component Object Model
 ******************************************************************************/

#ifndef MINICOM_H
#define MINICOM_H

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/

#if defined(XP_PC) && !defined(XP_OS2)
#if defined(_WIN32)
#include "objbase.h"
#else
#include <windows.h>
#include "compobj.h"
#endif
#else /* !XP_PC or XP_OS2*/

#if defined(XP_OS2)
#define TID OS2TID   /* global rename in OS2 H's!               */
#include <os2.h>
#undef TID           /* and restore                             */
#endif

typedef struct _GUID {
	long		Data1;
	short		Data2;
	short		Data3;
	char		Data4[8];
} GUID;

typedef GUID	IID;
typedef GUID	CLSID;

typedef IID*	REFIID;
typedef GUID*	REFGUID;
typedef CLSID*	REFCLSID;

#ifdef __cplusplus
#define EXTERN_C		extern "C"
#else
#define EXTERN_C
#endif /* cplusplus */

#ifndef INITGUID
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
	EXTERN_C const GUID name
#else
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
	EXTERN_C const GUID name \
                = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }
#endif /* INITGUID */

#endif /* !XP_PC or XP_OS2*/

#define JRI_DEFINE_GUID(name, l, w1, w2) \
	DEFINE_GUID(name, l, w1, w2,		 \
				0xbb, 0x58, 0x00, 0x80, 0x5f, 0x74, 0x03, 0x79)

typedef long
(*MCOM_QueryInterface_t)(void* self, REFIID id, void* *result);

typedef long
(*MCOM_AddRef_t)(void* self);

typedef long
(*MCOM_Release_t)(void* self);

#if !defined(XP_PC) || defined(XP_OS2)

typedef struct IUnknown {
    MCOM_QueryInterface_t	QueryInterface;
    MCOM_AddRef_t			AddRef;
    MCOM_Release_t			Release;
} IUnknown;

#define IUnknown_QueryInterface(self, interfaceID, resultingInterface)		\
		(((*((IUnknown**)self))->QueryInterface)(self, interfaceID, resultingInterface))

#define IUnknown_AddRef(self)							\
		(((*((IUnknown**)self))->AddRef)(self))

#define IUnknown_Release(self)							\
		(((*((IUnknown**)self))->Release)(self))

#endif /* !XP_PC or XP_OS2*/

typedef long
(*MCOM_CreateInstance_t)(void* self, IUnknown* outer, REFIID interfaceID,
						 void** newInstance);

typedef long
(*MCOM_LockServer_t)(void* self, int doLock);

/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif /* MINICOM_H */
/******************************************************************************/
