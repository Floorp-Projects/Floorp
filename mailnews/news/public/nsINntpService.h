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

#ifndef nsINntpService_h___
#define nsINntpService_h___

#include "nscore.h"
#include "nsISupports.h"
#include "nsFileSpec.h"

/* 4C9F90E0-E19B-11d2-806E-006008128C4E} */
#define NS_INNTPSERVICE_IID                         \
{ 0x4c9f90e0, 0xe19b, 0x11d2,						\
    { 0x80, 0x63, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

/* 4C9F90E1-E19B-11d2-806E-006008128C4E */
#define NS_NNTPSERVICE_CID							\
{ 0x4c9f90e1, 0xe19b, 0x11d2,						\
	{0x80, 0x6e, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e} }

////////////////////////////////////////////////////////////////////////////////////////
// The Nntp Service is an interfaced designed to make building and running nntp urls
// easier. I'm not sure if this service will go away when the new networking model comes
// on line (as part of the N2 project). So I reserve the right to change my mind and take
// this service away =).
////////////////////////////////////////////////////////////////////////////////////////
class nsIURL;
class nsIUrlListener;

class nsINntpService : public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_INNTPSERVICE_IID; return iid; }
	
	/////////////////////////////////////////////////////////////////////////////////////
	// We should have separate interface methods for each type of desired nntp action
	// but for now I'm just passing in the urlSpec directly from the caller.
	// (i.e we should have a method for downloading new news articles, discovering news
	// groups for a host, etc.....
	/////////////////////////////////////////////////////////////////////////////////////

	NS_IMETHOD RunNewsUrl (const nsString& urlString, nsISupports * aConsumer, 
						   nsIUrlListener * aUrlListener, nsIURL ** aURL) = 0;
};

#endif /* nsINntpService_h___ */
