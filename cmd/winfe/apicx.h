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

#ifndef _APICX_H
#define _APICX_H

#ifndef __APIAPI_H
	#include "apiapi.h"
#endif
#ifndef __NSGUIDS_H
	#include "nsguids.h"
#endif

// Define these for graceful type coping

typedef struct MWContext_ MWContext;
typedef struct URLStruct_ URLStruct;
typedef struct PrintSetup_ PrintSetup;

class IMWContext: public IUnknown
{
public:

    //  The context functions of the class, all pure virtual at this level.
    //  This is a special hack to autogenerate the functions as pure virtual.
    //  See the header mk_cx_fn.h for more insight.
    //  This more or less must be the first thing done in the class, to get all the
    //      virtual tables correct in classes to come.

#define FE_DEFINE(funkage, returns, args) virtual returns funkage args = 0;
#include "mk_cx_fn.h"

    //  The URL exit routine.
    //  This is called by CFE_GetUrlExitRoutine, which should be passed as the exit routine
    //      in any calls to NET_GetURL....
    virtual void GetUrlExitRoutine(URL_Struct *pUrl, int iStatus, MWContext *pContext) = 0;

    //  The text translation exit routine.
    //  This is called by CFE_TextTranslationExitRoutine, which should be passed as the exit
    //      routine in any calls to XL_TranslateText....
    virtual void TextTranslationExitRoutine(PrintSetup *pTextFE) = 0;

	//  XP Context accessor
	virtual MWContext *GetContext() const = 0;

	//	This function is very important.
	//	It allows you to write code which can detect when a context
	//		is actually in the process of being destroyed.
	//	In many cases, you will receive callbacks from the XP libraries
	//		while the context is being destroyed.  If you GPF, then use
	//		this function to better detect your course of action.
	virtual BOOL IsDestroyed() const = 0;

	//	Function to load a URL with this context, with the custom exit routing handler.
	//	Use this to NET_GetURL instead or XL_TranslateText instead.
	virtual int GetUrl(URL_Struct *pUrl, FO_Present_Types iFormatOut, BOOL bReallyLoad = TRUE, BOOL bForceNew = FALSE) = 0;

	//	Generic brain dead interface to load a URL.
	//	This used to be known as OnNormalLoad.
	virtual int NormalGetUrl(const char *pUrl, const char *pReferrer = NULL, const char *pTarget = NULL, BOOL bForceNew = FALSE) = 0;
};

typedef IMWContext *LPMWCONTEXT;

#endif
