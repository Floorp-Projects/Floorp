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

#ifndef nsISmtpService_h___
#define nsISmtpService_h___

#include "nscore.h"
#include "nsISupports.h"
#include "nsFileSpec.h"
#include "nsISupportsArray.h"
#include "nsISmtpServer.h"

/* FBAF0F10-CA9B-11d2-8063-006008128C4E */

#define NS_ISMTPSERVICE_IID                         \
{ 0xfbaf0f10, 0xca9b, 0x11d2,                  \
    { 0x80, 0x63, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

#define NS_SMTPSERVICE_CID						  \
{ /* 5B6419F1-CA9B-11d2-8063-006008128C4E */      \
 0x5b6419f1, 0xca9b, 0x11d2,                      \
 {0x80, 0x63, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e}}

/////////////////////////////////////////////////////////////////////////////
// The Smtp Service is an interfaced designed to make building and running
// mail to urls easier. I'm not sure if this service will go away when the
// new networking model comes on line (as part of the N2 project). So I
// reserve the right to change my mind and take this service away =).
/////////////////////////////////////////////////////////////////////////////
class nsIURI;
class nsIUrlListener;

class nsISmtpService : public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_ISMTPSERVICE_IID; return iid; }
	
  ///////////////////////////////////////////////////////////////////////////
  // SendMailMessage requires the file name of the message to send, the
  // sender, a comma delimited list of recipients.
  // It builds an Smtp url, makes an smtp connection and runs the url. If you
  // want a handle on the running task, pass in  a valid nsIURI ptr. You can
  // later interrupt this action by asking the netlib service manager to
  // interrupt the url you are given back. Remember to release aURL when you
  // are done with it. Pass nsnull in for aURL if you don't care about 
  // the returned URL.
  //
  // If you don't care about listening to the url, feel free to pass in
  // nsnull for that argument.
  //
  // You can also pass an SMTP server as an argument if you want to send
  // this message with a specific server.. otherwise it will use the
  // default server
  //////////////////////////////////////////////////////////////////////////

  NS_IMETHOD SendMailMessage(const nsFilePath& aFilePath,
                             const nsString& aRecipients, 
                             nsIUrlListener * aUrlListener,
                             nsISmtpServer *aServer,
                             nsIURI ** aURL) = 0;

  NS_IMETHOD GetSmtpServers(nsISupportsArray ** aResult) = 0;
  NS_IMETHOD GetDefaultSmtpServer(nsISmtpServer **aServer) = 0;
  NS_IMETHOD SetDefaultSmtpServer(nsISmtpServer *aServer) = 0;
};

#endif /* nsISmtpService_h___ */
