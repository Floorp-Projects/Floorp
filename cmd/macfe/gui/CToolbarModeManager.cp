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

// CToolbarModeManager.cp

#include "CToolbarModeManager.h"

#include <UException.h>

#include "CToolbarPrefsProxy.h"

LBroadcaster* CToolbarModeManager::sBroadcaster = NULL;
CToolbarPrefsProxy* CToolbarModeManager::sPrefsProxy = NULL;

#pragma mark -- CToolbarModeManager --

int CToolbarModeManager::BroadcastToolbarModeChange(
	const char* /* domain */,
	void* /* instance_data */)
{
	Int32 toolbarStyle;
	int result = GetToolbarPref(toolbarStyle);
	if (result == noErr)
	{
		if (sBroadcaster)
			sBroadcaster->BroadcastMessage(msg_ToolbarModeChange,
										   (void*)toolbarStyle);
	}
	return result;
}

void CToolbarModeManager::AddToolbarModeListener(LListener* inListener)
{
	try {
		if (!sBroadcaster)
			sBroadcaster = new LBroadcaster;
		sBroadcaster->AddListener(inListener);
	} catch (...) {
		sBroadcaster = NULL;
	}
}

int CToolbarModeManager::GetToolbarPref(Int32& ioPref)
{
	int result = noErr;
	if (sPrefsProxy)
		result = sPrefsProxy->GetToolbarPref(ioPref);
	else
		ioPref = defaultToolbarMode;
	return result;
}

#pragma mark -- CToolbarModeChangeListener --

CToolbarModeChangeListener::CToolbarModeChangeListener()
{
	CToolbarModeManager::AddToolbarModeListener(this);
}

void CToolbarModeChangeListener::ListenToMessage(MessageT inMessage, void* ioParam)
{
	if (inMessage == CToolbarModeManager::msg_ToolbarModeChange)
		HandleModeChange((Int8)ioParam);
}
