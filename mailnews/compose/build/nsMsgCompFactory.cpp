/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "msgCore.h"

#include "nsISupports.h"
#include "nsCOMPtr.h"

#include "nsIFactory.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsIModule.h"

#include "pratom.h"
#include "nsMsgCompCID.h"



/* Include all of the interfaces our factory can generate components for */
#include "nsMsgSendFact.h"
#include "nsMsgCompFieldsFact.h"
#include "nsMsgSendLaterFact.h"

#include "nsMsgComposeFact.h"
#include "nsMsgSendLater.h"
#include "nsSmtpUrl.h"
#include "nsISmtpService.h"
#include "nsSmtpService.h"
#include "nsMsgComposeService.h"
#include "nsMsgCompose.h"
#include "nsMsgSend.h"
#include "nsMsgQuote.h"
#include "nsIMsgDraft.h"
#include "nsMsgCreate.h"    // For drafts...I know, awful file name...
#include "nsSmtpServer.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsSmtpService);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSmtpServer);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgCompose);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgCompFields);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgComposeAndSend);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgSendLater);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgDraft)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgComposeService);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgQuote);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgQuoteListener);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSmtpUrl);

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////
static PRInt32 g_InstanceCount = 0;
static PRInt32 g_LockCount = 0;

static nsModuleComponentInfo components[] =
{
  { NS_MSGCOMPOSE_CID,        &nsMsgComposeConstructor,  NS_MSGCOMPOSE_PROGID,  },
  { NS_MSGCOMPOSESERVICE_CID, &nsMsgComposeServiceConstructor, NS_MSGCOMPOSESERVICE_PROGID, },
  { NS_MSGCOMPFIELDS_CID,     &nsMsgCompFieldsConstructor, NS_MSGCOMPFIELDS_PROGID, },
  { NS_MSGDRAFT_CID,          &nsMsgDraftConstructor, NS_MSGDRAFT_PROGID, },
  { NS_MSGSEND_CID,           &nsMsgComposeAndSendConstructor, NS_MSGSEND_PROGID, },
  { NS_MSGSENDLATER_CID,      &nsMsgSendLaterConstructor, NS_MSGSENDLATER_PROGID, },
  { NS_SMTPSERVICE_CID,       &nsSmtpServiceConstructor, NS_SMTPSERVICE_PROGID,  },
  { NS_SMTPSERVICE_CID,       &nsSmtpServiceConstructor, NS_MAILTOHANDLER_PROGID, },
  { NS_SMTPSERVER_CID,        &nsSmtpServerConstructor, NS_SMTPSERVER_PROGID,    },
  { NS_SMTPURL_CID,           &nsSmtpUrlConstructor, NS_SMTPURL_PROGID, },
  { NS_MSGQUOTE_CID,          &nsMsgQuoteConstructor, NS_MSGQUOTE_PROGID, },
  { NS_MSGQUOTELISTENER_CID,  &nsMsgQuoteListenerConstructor, NS_MSGQUOTELISTENER_PROGID, }
};

  
NS_IMPL_MODULE(nsMsgComposeModule, components)
NS_IMPL_NSGETMODULE(nsMsgComposeModule)
