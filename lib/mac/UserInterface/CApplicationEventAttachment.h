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

//	Usage Notes:
//
//	(1) Add this attachment as the first attachment to the application object.
//	(2) The EventRecord from ProcessNextEvent *must* persist across calls to
//		ProcessNextEvent in order to prevent dangling references to the event.

#ifndef	CApplicationEventAttachment_H
#define	CApplicationEventAttachment_H
#pragma once

#include <LAttachment.h>
#include <LApplication.h>

#ifndef __EVENTS__
#include <Events.h>
#endif

class CApplicationEventAttachment : public LAttachment
{
public:
							CApplicationEventAttachment();
	virtual 				~CApplicationEventAttachment();
						
	virtual	void			ExecuteSelf(
										MessageT			inMessage,
										void*				ioParam);

	static LApplication&	GetApplication();
	
	static EventRecord&		GetCurrentEvent();
	static Boolean			CurrentEventHasModifiers(
										EventModifiers		inModifiers);

	static void				ProcessNextEvent();

private:
	static LApplication*	sApplication;
	static EventRecord*		sCurrentEvent;
	static EventRecord		sFakeNullEvent;
	static Boolean			sFakeNullEventInitialized;
};

#endif