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

/*
 * intshcut.h - Internet Shortcut interface definitions.
 */


#ifndef __INTSHCUT_H__
#define __INTSHCUT_H__


/* Headers
 **********/

#include "isguids.h"


#ifdef __cplusplus
extern "C" {                        /* Assume C declarations for C++. */
#endif   /* __cplusplus */


/* Constants
 ************/

/* Define API decoration for direct import of DLL functions. */

#ifdef _INTSHCUT_
#define INTSHCUTAPI
#else
#define INTSHCUTAPI                 DECLSPEC_IMPORT
#endif

/* HRESULTs */

#define URL_E_INVALID_FLAGS         MAKE_SCODE(SEVERITY_ERROR, FACILITY_ITF, 0x1000)
#define URL_E_INVALID_SYNTAX        MAKE_SCODE(SEVERITY_ERROR, FACILITY_ITF, 0x1001)
#define URL_E_UNREGISTERED_PROTOCOL MAKE_SCODE(SEVERITY_ERROR, FACILITY_ITF, 0x1002)

#define IS_E_INVALID_FILE           MAKE_SCODE(SEVERITY_ERROR, FACILITY_ITF, 0x2000)
#define IS_E_INVALID_DIRECTORY      MAKE_SCODE(SEVERITY_ERROR, FACILITY_ITF, 0x2001)
#define IS_E_EXEC_FAILED            MAKE_SCODE(SEVERITY_ERROR, FACILITY_ITF, 0x2002)


/* Interfaces
 *************/

/* IUniformResourceLocator::SetURL() input flags */

typedef enum iurl_seturl_flags
{
   /* Guess protocol if missing.*/

   IURL_SETURL_FL_GUESS_PROTOCOL         = 0x0001,

   /* Use default protocol if missing.*/

   IURL_SETURL_FL_USE_DEFAULT_PROTOCOL = 0x0002,

   /* flag combinations */

   ALL_IURL_SETURL_FLAGS               = (IURL_SETURL_FL_GUESS_PROTOCOL |
                                          IURL_SETURL_FL_USE_DEFAULT_PROTOCOL)
}
IURL_SETURL_FLAGS;

/* IUniformResourceLocator::Open() input flags */

typedef enum iurl_open_flags
{
   /*
    * Allow interaction with user.  If set, hwndOwner is valid.  If clear,
    * hwndOwner is NULL.
    */

   IURL_OPEN_FL_ALLOW_UI               = 0x0001,

   /* flag combinations */

   ALL_IURL_OPEN_FLAGS                 = IURL_OPEN_FL_ALLOW_UI
}
IURL_OPEN_FLAGS;

#undef  INTERFACE
#define INTERFACE IUniformResourceLocator

DECLARE_INTERFACE_(IUniformResourceLocator, IUnknown)
{
   /* IUnknown methods */

   STDMETHOD(QueryInterface)(THIS_
                             REFIID riid,
                             PVOID *ppvObject) PURE;

   STDMETHOD_(ULONG, AddRef)(THIS) PURE;

   STDMETHOD_(ULONG, Release)(THIS) PURE;

   /* IUniformResourceLocator methods */

   /*
    * Success:
    *    S_OK
    *
    * Failure:
    *    E_OUTOFMEMORY
    *    URL_E_INVALID_SYNTAX
    */
   STDMETHOD(SetURL)(THIS_
                     PCSTR pcszURL,
                     DWORD dwFlags) PURE;

   /*
    * Success:
    *    S_OK
    *    S_FALSE
    *
    * Failure:
    *    E_OUTOFMEMORY
    */
   STDMETHOD(GetURL)(THIS_
                     PSTR *ppszURL) PURE;

   /*
    * Success:
    *    S_OK
    *
    * Failure:
    *    E_OUTOFMEMORY
    *    URL_E_INVALID_SYNTAX
    *    URL_E_UNREGISTERED_PROTOCOL
    *    IS_E_EXEC_FAILED
    */
   STDMETHOD(Open)(THIS_
                   HWND hwndOwner,
                   DWORD dwFlags) PURE;
};
typedef IUniformResourceLocator *PIUniformResourceLocator;
typedef const IUniformResourceLocator CIUniformResourceLocator;
typedef const IUniformResourceLocator *PCIUniformResourceLocator;


/* Prototypes
 *************/

/* TranslateURL() input flags */

typedef enum translateurl_flags
{
   /* Guess protocol if missing.*/

   TRANSLATEURL_FL_GUESS_PROTOCOL         = 0x0001,

   /* Use default protocol if missing.*/

   TRANSLATEURL_FL_USE_DEFAULT_PROTOCOL   = 0x0002,

   /* flag combinations */

   ALL_TRANSLATEURL_FLAGS                 = (TRANSLATEURL_FL_GUESS_PROTOCOL |
                                             TRANSLATEURL_FL_USE_DEFAULT_PROTOCOL)
}
TRANSLATEURL_FLAGS;

/*
 * Success:
 *    S_OK
 *    S_FALSE
 *
 * Failure:
 *    E_OUTOFMEMORY
 *    E_POINTER
 *    URL_E_INVALID_FLAGS
 */
INTSHCUTAPI HRESULT WINAPI TranslateURL(PCSTR pcszURL, DWORD dwFlags, PSTR *ppszTranslatedURL);


#ifdef __cplusplus
}                                   /* End of extern "C" {. */
#endif   /* __cplusplus */


#endif   /* ! __INTSHCUT_H__ */

