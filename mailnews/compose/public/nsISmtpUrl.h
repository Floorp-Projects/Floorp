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

#ifndef nsISmtpUrl_h___
#define nsISmtpUrl_h___

#include "nscore.h"
#include "nsIURL.h"

#include "nsISupports.h"

/* include all of our event sink interfaces */

/* 16ADF2F1-BBAD-11d2-804E-006008128C4E */

#define NS_ISMTPURL_IID                         \
{ 0x16adf2f1, 0xbbad, 0x11d2,                  \
    { 0x80, 0x4e, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

///////////////////////////////////////////////////////////////////////////////////
// There are really two types of mailto (Smtp) urls we can encounter. The first
// one is really a request to bring up the compose window. This url is encountered
// in web pages, etc. and has the following form:
//				mailto:TO_FIELD?FIELD1=VALUE1&FIELD2=VALUE2
// We automatically parse this url and break down its components (to, cc, subject,
// body, etc) when the url is created.
//
// There is a second kind of mailto url: the url that is fired when you actually
// want to post a message to your mail server. Since this url is always internal,
// you must call SetPostMessage. When the protocol goes to parse the url, if it sees
// that post message has been called on the url then it recognizes that the contents
// of this url should be posted to the server. If this function hasn't been set,
// we'll just bring up the  compose window.
// mscott --> we could break this down into two SMTP url classes....
//////////////////////////////////////////////////////////////////////////////////////
class nsISmtpUrl : public nsIURL
{
public:
    static const nsIID& IID() { static nsIID iid = NS_ISMTPURL_IID; return iid; }

	/////////////////////////////////////////////////////////////////////////////// 
	// SMTP Parse specific getters --> retrieves portions from the url spec...
	///////////////////////////////////////////////////////////////////////////////
	
	// mscott: I used to have individual getters for ALL of these fields but it was
	// getting way out of hand...besides in the actual protocol, we want all of these
	// fields anyway so why go through the extra step of making the protocol call
	// 12 get functions...
	NS_IMETHOD GetMessageContents(const char ** aToPart, const char ** aCcPart, const char ** aBccPart, 
		const char ** aFromPart, const char ** aFollowUpToPart, const char ** aOrganizationPart, 
		const char ** aReplyToPart, const char ** aSubjectPart, const char ** aBodyPart, const char ** aHtmlPart, 
		const char ** aReferencePart, const char ** aAttachmentPart, const char ** aPriorityPart, 
		const char ** aNewsgroupPart, const char ** aNewsHostPart, PRBool * aforcePlainText) = 0;

	// Caller must call PR_FREE on list when it is done with it. This list is a list of all
	// recipients to send the email to. each name is NULL terminated...
	NS_IMETHOD GetAllRecipients(char ** aRecipientsList) = 0;

	// is the url a post message url or a bring up the compose window url? 
	NS_IMETHOD IsPostMessage(PRBool * aPostMessage) = 0; 
	
	// used to set the url as a post message url...
	NS_IMETHOD SetPostMessage(PRBool aPostMessage) = 0;

	// the message can be stored in a file....allow accessors for getting and setting
	// the file name to post...
	NS_IMETHOD SetPostMessageFile(const char * aFileName) = 0;
	NS_IMETHOD GetPostMessageFile(const char ** aFileName) = 0;

	/////////////////////////////////////////////////////////////////////////////// 
	// SMTP Url instance specific getters and setters --> info the protocol needs
	// to know in order to run the url...these are NOT event sinks which are things
	// the caller needs to know...
	///////////////////////////////////////////////////////////////////////////////

	// by default the url is really a bring up the compose window mailto url...
	// you need to call this function if you want to force the message to be posted
	// to the mailserver...

	// mscott -- when we have identities it would be nice to just have an identity 
	// interface here that would encapsulte things like username, domain, password,
	// etc...
	NS_IMETHOD GetUserEmailAddress(const char ** aUserName) = 0;
	NS_IMETHOD SetUserEmailAddress(const char * aUserName) = 0;
	NS_IMETHOD GetUserPassword(const char ** aUserPassword) = 0;
	NS_IMETHOD SetUserPassword(const char * aUserPassword) = 0;

	/////////////////////////////////////////////////////////////////////////////// 
	// Getters and Setters for the smtp specific event sinks to bind to to your url
	///////////////////////////////////////////////////////////////////////////////

	// mscott: this interface really belongs in nsIURL and I will move it there after talking
	// it over with core netlib. This error message replaces the err_msg which was in the 
	// old URL_struct. Also, it should probably be a nsString or a PRUnichar *. I don't know what
	// XP_GetString is going to return in mozilla. 

	NS_IMETHOD SetErrorMessage (char * errorMessage) = 0;
	// caller must free using PR_FREE
	NS_IMETHOD GetErrorMessage (char ** errorMessage) const = 0;
};

#endif /* nsISmtpUrl_h___ */
