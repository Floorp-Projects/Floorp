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

//	StBlockingDialogHandler.h

#pragma once

#include <UModalDialogs.h>

class StBlockingDialogHandler : public StDialogHandler
{
	public:
							StBlockingDialogHandler(
									ResIDT 			inDialogResID,
									LCommander*		inSuper);
	
		virtual				~StBlockingDialogHandler();

		virtual Boolean		ExecuteAttachments(
									MessageT		inMessage,
									void			*ioParam);
									
		virtual void		FindCommandStatus(CommandT inCommand,
									Boolean &outEnabled, Boolean &outUsesMark,
									Char16 &outMark, Str255 outName);
};
