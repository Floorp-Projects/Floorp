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

#include "CDesktop.h"

#include <LWindow.h>
#include <UWindows.h>

// ---------------------------------------------------------------------------
//		¥ FetchNextWindowWithThisAttribute
// ---------------------------------------------------------------------------

LWindow*
CDesktop::FetchNextWindowWithThisAttribute(
	const LWindow&	inWindow,
	EWindAttr		inAttribute)
{
		// Start with specified window and search thru window list until finding the
		// next window with the specified attributes.
		
	WindowPtr	macWindowP			= static_cast<WindowPtr>(inWindow.GetMacPort());
	WindowPtr	specifiedMacWindowP	= macWindowP;
	
	LWindow*	theWindow = nil;
	while ((theWindow = LWindow::FetchWindowObject(macWindowP)) != nil)
	{
		if (theWindow->HasAttribute(inAttribute)	&&
			 ((WindowPeek) macWindowP)->visible		&&
			 macWindowP != specifiedMacWindowP)
		{
			break;
		}
		macWindowP = (WindowPtr) ((WindowPeek) macWindowP)->nextWindow;
	}

	if (macWindowP == specifiedMacWindowP)
	{
		theWindow = nil;
	}
	
	return theWindow;
}

// ---------------------------------------------------------------------------
//		¥ FetchNextRegular
// ---------------------------------------------------------------------------

LWindow*
CDesktop::FetchNextRegular(
	const LWindow&	inWindow)
{
	return FetchNextWindowWithThisAttribute(inWindow, windAttr_Regular);
}

// ---------------------------------------------------------------------------
//		¥ FindWindow
// ---------------------------------------------------------------------------

LWindow*
CDesktop::FindWindow(
	ResIDT			inWINDid)
{
	Int16	theIndex = 1;
	LWindow*	theWindow = LWindow::FetchWindowObject(UWindows::FindNthWindow(theIndex));
	
	while (theWindow && theWindow->GetPaneID() != inWINDid)
	{
		theIndex++;
		theWindow = LWindow::FetchWindowObject(UWindows::FindNthWindow(theIndex));
	}
	
	return theWindow;
}