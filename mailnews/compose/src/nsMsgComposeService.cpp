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

#include "nsMsgComposeService.h"
#include "nsMsgCompCID.h"
#include "nsISupportsArray.h"
#include "nsIServiceManager.h"
#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"
#include "nsINetService.h"
#include "nsIWebShellWindow.h"
#include "nsIWebShell.h"
#include "nsAppCoresCIDs.h"
#include "nsIDOMToolkitCore.h"

static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_CID(kToolkitCoreCID, NS_TOOLKITCORE_CID);
static NS_DEFINE_CID(kMsgComposeCID, NS_MSGCOMPOSE_CID);

nsMsgComposeService::nsMsgComposeService()
{
	nsresult rv;

	NS_INIT_REFCNT();
    rv = NS_NewISupportsArray(getter_AddRefs(m_msgQueue));
}


nsMsgComposeService::~nsMsgComposeService()
{
}


/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS(nsMsgComposeService, nsMsgComposeService::GetIID());

nsresult nsMsgComposeService::OpenComposeWindow(const PRUnichar *msgComposeWindowURL, const PRUnichar *originalMsgURI, MSG_ComposeType type, MSG_ComposeFormat format)
{
	nsAutoString args = "";
	nsresult rv;

	NS_WITH_SERVICE(nsIDOMToolkitCore, toolkitCore, kToolkitCoreCID, &rv); 
    if (NS_FAILED(rv))
		return rv;

	args.Append("type=");
	args.Append(type);
	args.Append(",");

	args.Append("format=");
	args.Append(format);

	if (originalMsgURI && *originalMsgURI)
	{
		args.Append(",originalMsg='");
		args.Append(originalMsgURI);
		args.Append("'");
	}
	
	if (msgComposeWindowURL && *msgComposeWindowURL)
		toolkitCore->ShowWindowWithArgs(msgComposeWindowURL, nsnull, args);
	else
		toolkitCore->ShowWindowWithArgs("chrome://messengercompose/content/", nsnull, args);
	
	return rv;
}


nsresult nsMsgComposeService::InitCompose(nsIDOMWindow *aWindow, const PRUnichar *originalMsgURI, PRInt32 type, PRInt32 format, nsIMsgCompose **_retval)
{
	nsresult rv;
	nsIMsgCompose * msgCompose = nsnull;
	
	rv = nsComponentManager::CreateInstance(kMsgComposeCID, nsnull,
	                                        nsIMsgCompose::GetIID(),
	                                        (void **) &msgCompose);
	if (NS_SUCCEEDED(rv) && msgCompose)
	{
		msgCompose->Initialize(aWindow, originalMsgURI, type, format);
		m_msgQueue->AppendElement(msgCompose);
		*_retval = msgCompose;
	}
	
	return rv;
}


nsresult nsMsgComposeService::DisposeCompose(nsIMsgCompose *compose, PRBool closeWindow)
{
	PRInt32 i = m_msgQueue->IndexOf(compose);
	if (i >= 0)
	{
		m_msgQueue->RemoveElementAt(i);
		
		if (closeWindow)
			;//TODO
		NS_RELEASE(compose);
	}
	return NS_OK;
}

