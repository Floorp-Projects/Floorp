/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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