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

#include "CRDFNotificationHandler.h"

HT_Notification
CRDFNotificationHandler::CreateNotificationStruct()
{
	HT_Notification notifyStruct = new HT_NotificationStruct;
	memset(notifyStruct, 0x00, sizeof(notifyStruct));

	notifyStruct->notifyProc = CRDFNotificationHandler::rdfNotifyProc;
	notifyStruct->data = (void *)this;
	
	return notifyStruct;
}

HT_Pane
CRDFNotificationHandler::CreateHTPane()
{
	HT_Notification notifyStruct = CreateNotificationStruct();
	if (notifyStruct)
		return HT_NewPane(notifyStruct);
	else
		return NULL;
}

void
CRDFNotificationHandler::rdfNotifyProc(
	HT_Notification	notifyStruct,
	HT_Resource		node,
	HT_Event		event)
{
	CRDFNotificationHandler* handler =
		reinterpret_cast<CRDFNotificationHandler*>(notifyStruct->data);
	handler->HandleNotification(notifyStruct, node, event);
}
