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
//	CWindowMediator.cp
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#ifdef PowerPlant_PCH
#include PowerPlant_PCH
#endif

#include "CWindowMediator.h"

#include "CCloseAllAttachment.h"	// MGY: added CCloseAllAttachment stuff
#include "resgui.h"					// MGY: added CCloseAllAttachment stuff
#include "CBrowserWindow.h"

CAutoPtr<CWindowMediator> CWindowMediator::sMediator;

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CMediatedWindow::CMediatedWindow(LStream* inStream, DataIDT inWindowType)
	:	LWindow(inStream)
{
	mWindowType = inWindowType;
	(CWindowMediator::GetWindowMediator())->NoteWindowCreated(this);

	// MGY: added CCloseAllAttachment stuff

	if (HasAttribute(windAttr_Regular | windAttr_CloseBox))
	{
		AddAttachment(new CCloseAllAttachment(CLOSE_WINDOW, CLOSE_ALL_WINDOWS));
	}
}

CMediatedWindow::~CMediatedWindow()
{
	(CWindowMediator::GetWindowMediator())->NoteWindowDisposed(this);
}

DataIDT CMediatedWindow::GetWindowType(void) const
{
	return mWindowType;
}

void CMediatedWindow::SetWindowType(DataIDT inWindowType)
{
	mWindowType = inWindowType;
}

void CMediatedWindow::SetDescriptor(ConstStr255Param inDescriptor)
{
	LWindow::SetDescriptor(inDescriptor);
	(CWindowMediator::GetWindowMediator())->NoteWindowDescriptorChanged(this);
}

void CMediatedWindow::ActivateSelf(void)
{
	LWindow::ActivateSelf();
	(CWindowMediator::GetWindowMediator())->NoteWindowActivated(this);
}

void CMediatedWindow::DeactivateSelf(void)
{
	LWindow::DeactivateSelf();
	(CWindowMediator::GetWindowMediator())->NoteWindowDeactivated(this);
}	
		
void CMediatedWindow::Hide(void)
{
	LWindow::Hide();
	(CWindowMediator::GetWindowMediator())->NoteWindowHidden(this);
}	
		
void CMediatedWindow::Show(void)
{
	LWindow::Show();
	(CWindowMediator::GetWindowMediator())->NoteWindowShown(this);
}	
		
void
CMediatedWindow::NoteWindowMenubarModeChanged()
{
	(CWindowMediator::GetWindowMediator())->NoteWindowMenubarModeChanged(this);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CWindowIterator::CWindowIterator(DataIDT inWindowType, Boolean inCountHidden)
:	mWindowType(inWindowType)
,	mIndexWindow(NULL)
,	mCountHidden(inCountHidden)
{
}
	
Boolean CWindowIterator::Next(CMediatedWindow*& outWindow)
{
	WindowPtr theStartWindow;
	if (mIndexWindow == NULL)
		theStartWindow = (WindowPtr)LMGetWindowList();
	else
	{
		theStartWindow = mIndexWindow->GetMacPort();
		theStartWindow = (WindowPtr) ((WindowPeek) theStartWindow)->nextWindow;
	}
		
	CMediatedWindow *theWindow, *theFoundWindow = NULL;
	for ( ; theStartWindow != NULL; theStartWindow = (WindowPtr)((WindowPeek)theStartWindow)->nextWindow)
	{
		theWindow = dynamic_cast<CMediatedWindow*>(LWindow::FetchWindowObject(theStartWindow));
		if (theWindow == NULL || (!mCountHidden && !theWindow->IsVisible()))
			continue;

		if ((mWindowType == WindowType_Any || theWindow->GetWindowType() == mWindowType))
		{
			theFoundWindow = theWindow;
			break;
		}
	}
		
	outWindow = theFoundWindow;
	mIndexWindow = theFoundWindow;
	
	return (theFoundWindow != NULL);	
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CWindowMediator* CWindowMediator::GetWindowMediator(void)
{
	if (!sMediator.get())
	{
		sMediator.reset(new CWindowMediator);
	}
	
	return sMediator.get();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Int16 CWindowMediator::CountOpenWindows(Int32 inWindType)
{
	return CountOpenWindows(inWindType, dontCareLayerType);
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ Count windows of a type in a layer
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
Int16 CWindowMediator::CountOpenWindows(Int32 inWindType, LayerType inLayerType, Boolean inIncludeInvisible)
{
	Int16 theWindowCount = 0;
	
	CMediatedWindow *theWindow = NULL;
	WindowPtr macWindowP = (WindowPtr) LMGetWindowList();
	while (macWindowP != NULL)
		{
		if ((theWindow = dynamic_cast<CMediatedWindow*>(LWindow::FetchWindowObject(macWindowP))) == NULL)
			{
			macWindowP = (WindowPtr) ((WindowPeek) macWindowP)->nextWindow;
			continue;
			}

		if ((inWindType == WindowType_Any) || (theWindow->GetWindowType() == inWindType))
			{
			if (inIncludeInvisible || theWindow->IsVisible())
				{
				switch (inLayerType)
					{
					case dontCareLayerType:
						theWindowCount++;
						break;
					case modalLayerType:
						if (theWindow->HasAttribute(windAttr_Modal))
							theWindowCount++;
						break;
					case floatingLayerType:
						if (theWindow->HasAttribute(windAttr_Floating))
							theWindowCount++;
						break;
					case regularLayerType:
						if (theWindow->HasAttribute(windAttr_Regular))
							theWindowCount++;
						break;
					}
				}
			}
				 
		macWindowP = (WindowPtr) ((WindowPeek) macWindowP)->nextWindow;
		}
		
	return theWindowCount;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
			
CMediatedWindow* CWindowMediator::FetchTopWindow(Int32 inWindType)
{
	CMediatedWindow *theFoundWindow = NULL;

	CWindowIterator theIterator(inWindType);
	theIterator.Next(theFoundWindow);
	
	return theFoundWindow;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ Returns the topmost mediated window in a given layer
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
CMediatedWindow* CWindowMediator::FetchTopWindow(LayerType inLayerType)
{
	return FetchTopWindow(WindowType_Any, inLayerType);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ Returns the topmost mediated window of a given type in a given layer
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
CMediatedWindow* CWindowMediator::FetchTopWindow(Int32 inWindType, LayerType inLayerType)
{
	return FetchTopWindow(inWindType, inLayerType, true);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ Returns the topmost mediated window of a given type in a given layer, with
//    the option of excluding windows which are restricted targets (created by javascript).
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
CMediatedWindow* CWindowMediator::FetchTopWindow(Int32 inWindType, LayerType inLayerType, Boolean inIncludeRestrictedTargets)
{
	CMediatedWindow *theFoundWindow = NULL;

	CWindowIterator theIterator(inWindType);
	theIterator.Next(theFoundWindow);
	
	while (theFoundWindow != NULL)
		{
		if (!inIncludeRestrictedTargets && (inWindType == WindowType_Browser))
		{
			CBrowserWindow* theBrWn = dynamic_cast<CBrowserWindow*>(theFoundWindow);
			if ((theBrWn != nil) && theBrWn->IsRestrictedTarget())
			{
				theIterator.Next(theFoundWindow);
				continue;
			}
		}
		switch (inLayerType)
			{
			case dontCareLayerType:
				return theFoundWindow;
			case modalLayerType:
				if (theFoundWindow->HasAttribute(windAttr_Modal))
					return theFoundWindow;
				else break;
			case floatingLayerType:
				if (theFoundWindow->HasAttribute(windAttr_Floating))
					return theFoundWindow;
				else break;
			case regularLayerType:
				if (theFoundWindow->HasAttribute(windAttr_Regular))
					return theFoundWindow;
				else break;
			}
		theIterator.Next(theFoundWindow);
		}
	return theFoundWindow;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ Returns the bottommost mediated window.
//    If inIncludeAlwaysLowered is false, returns the window above the topmost alwaysLowered
//    window.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
CMediatedWindow* CWindowMediator::FetchBottomWindow(Boolean inIncludeAlwaysLowered)
{
	CMediatedWindow *theFoundWindow = NULL;
	CMediatedWindow *lastFoundWindow = NULL;

	CWindowIterator theIterator(WindowType_Any);
	theIterator.Next(theFoundWindow);
	
	while (theFoundWindow != NULL)
		{
		if (!inIncludeAlwaysLowered)
			{
			CBrowserWindow* theBrWn = dynamic_cast<CBrowserWindow*>(theFoundWindow);
			if ((theBrWn != nil) && theBrWn->IsAlwaysLowered())
				return lastFoundWindow;
			}
		lastFoundWindow = theFoundWindow;
		theIterator.Next(theFoundWindow);
		}
	return lastFoundWindow;
}

void CWindowMediator::CloseAllWindows(Int32 inWindType)
{
	CMediatedWindow* theWindow;
	
	CWindowIterator theIterator(inWindType);
	theIterator.Next(theWindow);
	
	while (theWindow != NULL)
		{
		CMediatedWindow* theNextWindow;
		theIterator.Next(theNextWindow);
		
		theWindow->AttemptClose();
		theWindow = theNextWindow;
		}
}

CWindowMediator::CWindowMediator()
{
}

CWindowMediator::~CWindowMediator()
{
}
	
void CWindowMediator::NoteWindowCreated(CMediatedWindow* inWindow)
{
	BroadcastMessage(msg_WindowCreated, inWindow);
	mWindowList.InsertItemsAt(1, LArray::index_First, &inWindow);
}

void CWindowMediator::NoteWindowDisposed(CMediatedWindow* inWindow)
{
	BroadcastMessage(msg_WindowDisposed, inWindow);
	mWindowList.Remove(&inWindow);
}

void CWindowMediator::NoteWindowShown(CMediatedWindow* inWindow)
{
	BroadcastMessage(msg_WindowCreated, inWindow);
}

void CWindowMediator::NoteWindowHidden(CMediatedWindow* inWindow)
{
	BroadcastMessage(msg_WindowDisposed, inWindow);
}

void CWindowMediator::NoteWindowDescriptorChanged(CMediatedWindow* inWindow)
{
	BroadcastMessage(msg_WindowDescriptorChanged, inWindow);
}

void CWindowMediator::NoteWindowActivated(CMediatedWindow* inWindow)
{
	BroadcastMessage(msg_WindowActivated, inWindow);
}

void CWindowMediator::NoteWindowDeactivated(CMediatedWindow* inWindow)
{
	BroadcastMessage(msg_WindowDeactivated, inWindow);
}

void CWindowMediator::NoteWindowMenubarModeChanged(CMediatedWindow* inWindow)
{
	BroadcastMessage(msg_WindowMenuBarModeChanged, inWindow);
}

