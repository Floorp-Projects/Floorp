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

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CURLCaption.cp
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#include "CURLCaption.h"
#include "CNSContext.h"
#include "PascalString.h"
#include "CURLEditField.h"

#include "net.h"
#include "resgui.h"
#include "uerrmgr.h"

CURLCaption::CURLCaption(LStream* inStream) :
LCaption(inStream)
{
}

void CURLCaption::ListenToMessage(MessageT inMessage, void* ioParam)
{
	switch(inMessage)
	{
		case msg_NSCLayoutNewDocument:
			if (ioParam)
			{
				URL_Struct* theURL = (URL_Struct*)ioParam;
				if (theURL->is_netsite)
					SetDescriptor(GetPString(NETSITE_RESID));
				else
					SetDescriptor(GetPString(LOCATION_RESID));
			}
			break;
		case msg_UserChangedURL:
			SetDescriptor(GetPString(GOTO_RESID));
			break;
	}
}
