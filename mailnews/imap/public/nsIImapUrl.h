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

#ifndef nsIImapUrl_h___
#define nsIImapUrl_h___

#include "nscore.h"
#include "MailNewsTypes.h"
#include "nsIMsgMailNewsUrl.h"

#include "nsISupports.h"
#include "nsFileSpec.h"

/* include all of our event sink interfaces */
#include "nsIImapLog.h"

/* 21A89610-DC0D-11d2-806C-006008128C4E */

#define NS_IIMAPURL_IID                     \
{ 0x21a89610, 0xdc0d, 0x11d2,                  \
    { 0x80, 0x6c, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

/* 21A89611-DC0D-11d2-806C-006008128C4E */

#define NS_IMAPURL_CID                      \
{ 0x21a89611, 0xdc0d, 0x11d2,                  \
    { 0x80, 0x6c, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

#define IMAP_PORT 143

typedef enum {
	nsImapActionSendText = 0      // a state used for testing purposes to send raw url text straight to the server....
} nsImapAction;

class nsIImapUrl : public nsIMsgMailNewsUrl
{
public:
  static const nsIID& GetIID() {
    static nsIID iid = NS_IIMAPURL_IID;
    return iid;
  }

	/////////////////////////////////////////////////////////////////////////////// 
	// Getters and Setters for the imap specific event sinks to bind to to your url
	///////////////////////////////////////////////////////////////////////////////
	NS_IMETHOD GetImapLog(nsIImapLog ** aImapLog) = 0;
	NS_IMETHOD SetImapLog(nsIImapLog  * aImapLog) = 0;

	/////////////////////////////////////////////////////////////////////////////// 
	// Getters and Setters for the imap url state
	///////////////////////////////////////////////////////////////////////////////

	NS_IMETHOD GetImapAction(nsImapAction * aImapAction) = 0;
	NS_IMETHOD SetImapAction(nsImapAction aImapAction) = 0;
};

#endif /* nsIImapUrl_h___ */
