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

#ifndef nsIMsgMailNewsUrl_h___
#define nsIMsgMailNewsUrl_h___

#include "nscore.h"
#include "nsISupports.h"


/* 6CFFCEB0-CB8C-11d2-8065-006008128C4E */

#define NS_IMSGMAILNEWSURL_IID							\
{ 0x6cffceb0, 0xcb8c, 0x11d2,							\
    { 0x80, 0x65, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

///////////////////////////////////////////////////////////////////////////////////
// Okay, I found that all of the mail and news url interfaces needed to support
// several common interfaces (in addition to those provided through nsIURL). 
// So I decided to group them all in this interface.
// This interface may grow or it may get smaller (if these things get pushed up
// into nsIURL). The unfortunate thing is that we'd also like to have all of our
// mail news protocols inherit implementations of this interface. But that is
// implementation inheritance across dlls. We could do it though....something else
// to add to the list =).
//////////////////////////////////////////////////////////////////////////////////

class nsIMsgMailNewsUrl : public nsISupports
{
public:
    static const nsIID& IID() { static nsIID iid = NS_IMSGMAILNEWSURL_IID; return iid; }

	/////////////////////////////////////////////////////////////////////////////// 
	// Getters and Setters for the nsIMsgMailNewsUrl specific info....
	///////////////////////////////////////////////////////////////////////////////

	NS_IMETHOD SetErrorMessage (char * errorMessage) = 0;
	// caller must free using PR_FREE
	NS_IMETHOD GetErrorMessage (char ** errorMessage) const = 0;

	// If the application wants to check to see if the url is currently being run
	// or not, it can us GetRunningUrlFlag. Protocol connections typically set this
	// value when they are actually processing the url. Why might an application care?
	// Let's say I'm writing a test harness and I want to prompt the user for another 
	// command after the url is done being run?
	NS_IMETHOD SetRunningUrlFlag(PRBool runningUrl) = 0;
	NS_IMETHOD GetRunningUrlFlag(PRBool *runningUrl) = 0;
};

#endif /* nsIMsgMailNewsUrl_h___ */