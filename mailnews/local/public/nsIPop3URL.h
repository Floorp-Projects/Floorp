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

#ifndef nsIPop3URL_h___
#define nsIPop3URL_h___

#include "nscore.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsIPop3Sink.h"

#include "nsISupports.h"

/* include all of our event sink interfaces */

/* 73c043d0-b7e2-11d2-ab5c-00805f8ac968 */

#define NS_IPOP3URL_IID                         \
{ 0x73c043d0, 0xb7e2, 0x11d2,                   \
    { 0xab, 0x5c, 0x0, 0x80, 0x5f, 0x8a, 0xc9, 0x68 } }

/* EA1B0A11-E6F4-11d2-8070-006008128C4E */

#define NS_POP3URL_CID                         \
{ 0xea1b0a11, 0xe6f4, 0x11d2,                   \
    { 0x80, 0x70, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

class nsIPop3URL : public nsIMsgMailNewsUrl
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IPOP3URL_IID; return iid; }

	/////////////////////////////////////////////////////////////////////////////// 
	// Getters and Setters for the news specific event sinks to bind to to your url
	///////////////////////////////////////////////////////////////////////////////

	// mscott: this interface really belongs in nsIURL and I will move it there after talking
	// it over with core netlib. This error message replaces the err_msg which was in the 
	// old URL_struct. Also, it should probably be a nsString or a PRUnichar *. I don't know what
	// XP_GetString is going to return in mozilla. 

    NS_IMETHOD SetPop3Sink(nsIPop3Sink* aPop3Sink) = 0;
    NS_IMETHOD GetPop3Sink(nsIPop3Sink** aPop3Sink) const = 0;

	NS_IMETHOD SetErrorMessage (char * errorMessage) = 0;
	// caller must free using PR_FREE
	NS_IMETHOD GetErrorMessage (char ** errorMessage) const = 0;
};

#endif /* nsIPop3URL_h___ */
