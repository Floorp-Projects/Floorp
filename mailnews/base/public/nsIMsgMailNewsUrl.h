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
#include "nsIURL.h"
#include "nsIUrlListener.h"

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

class nsIMsgMailNewsUrl : public nsIURL
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IMSGMAILNEWSURL_IID; return iid; }

	///////////////////////////////////////////////////////////////////////////////
	// Eventually we'd like to push this type of functionality up into nsIURL.
	// The idea is to allow the "application" (the part of the code which wants to 
	// run a url in order to perform some action) to register itself as a listener
	// on url. As a url listener, the app will be informed when the url begins to run
	// and when the url is finished. 
	////////////////////////////////////////////////////////////////////////////////
	NS_IMETHOD RegisterListener (nsIUrlListener * aUrlListener) = 0;
	NS_IMETHOD UnRegisterListener (nsIUrlListener * aUrlListener) = 0;

	/////////////////////////////////////////////////////////////////////////////// 
	// Getters and Setters for the nsIMsgMailNewsUrl specific info....
	///////////////////////////////////////////////////////////////////////////////

	NS_IMETHOD SetErrorMessage (char * errorMessage) = 0;
	// caller must free using PR_FREE
	NS_IMETHOD GetErrorMessage (char ** errorMessage) const = 0;

	// if you really want to know what the current state of the url is (running or not
	// running) you should look into becoming a urlListener...
	NS_IMETHOD SetUrlState(PRBool runningUrl, nsresult aStatusCode) = 0;
	NS_IMETHOD GetUrlState(PRBool *runningUrl) = 0;
};

//////////////////////////////////////////////////////////////////////////////////
// This is a very small interface which I'm grouping with the mailnewsUrl interface.
// Several url types (mailbox, imap, nntp) have urls which can be have URI 
// equivalents. We want to provide the app the ability to get the URI for the 
// url. This URI to URL mapping doesn't exist for all mailnews urls...hence I'm
// grouping it into a separate interface...
//////////////////////////////////////////////////////////////////////////////////

/* 02338DD2-E7B9-11d2-8070-006008128C4E */

#define NS_IMSGURIURL_IID							\
{ 0x2338dd2, 0xe7b9, 0x11d2,							\
    { 0x80, 0x70, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

class nsIMsgUriUrl : public nsISupports
{
	static const nsIID& GetIID() { static nsIID iid = NS_IMSGURIURL_IID; return iid; }
	NS_IMETHOD GetURI(char ** aURI) = 0; 
};

#endif /* nsIMsgMailNewsUrl_h___ */
