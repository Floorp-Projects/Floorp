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
//	CWindowMediator.h
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#pragma once

#include <LBroadcaster.h>
#include <LWindow.h>

#include "CAutoPtr.h"

enum {
	WindowType_Any				=	'****'
};

const MessageT	msg_WindowCreated				=	'cre8';
const MessageT	msg_WindowDisposed				=	'dsp0';
const MessageT	msg_WindowDescriptorChanged		=	'dscG';
const MessageT	msg_WindowActivated				=	'wact';
const MessageT	msg_WindowDeactivated			=	'wdct';
const MessageT	msg_WindowMenuBarModeChanged	=	'wmbr';

#include "Netscape_Constants.h"

class CMediatedWindow : public LWindow
{
	public:
							CMediatedWindow(
									LStream* 	inStream,
									DataIDT		inWindowType);
									
		virtual				~CMediatedWindow();
	
		DataIDT				GetWindowType(void) const;
		void				SetWindowType(DataIDT inWindowType);

		virtual	void		SetDescriptor(ConstStr255Param inDescriptor);
		
		// I18N stuff
		// If the subclass of CMediatedWindow know the default csid for a new context
		// It should overload the DefaultCSIDForNewWindow() function and return non-0 value
		virtual Int16			DefaultCSIDForNewWindow(void) { return 0; };


	protected:

		virtual	void		ActivateSelf(void);
		virtual void		DeactivateSelf(void);
		
		void				NoteWindowMenubarModeChanged();
		
		DataIDT				mWindowType;
};


class CWindowIterator
{
	public:
							CWindowIterator(DataIDT inWindowType);
	
	Boolean					Next(CMediatedWindow*& outWindow);
	
	protected:

		CMediatedWindow*	mIndexWindow;
		DataIDT				mWindowType;
};

enum LayerType
{
	dontCareLayerType,
	modalLayerType,
	floatingLayerType,
	regularLayerType
};

class CWindowMediator : public LBroadcaster
{
	friend class CMediatedWindow;
	
	public:
		static CWindowMediator*				GetWindowMediator();

			// ¥ Window Fetching
		Int16				CountOpenWindows(Int32 inWindType);
		Int16				CountOpenWindows(Int32 inWindType, LayerType inLayerType, Boolean inIncludeInvisible = true);
		CMediatedWindow*	FetchTopWindow(Int32 inWindType);
		CMediatedWindow*	FetchTopWindow(LayerType inLayerType);
		CMediatedWindow*	FetchTopWindow(Int32 inWindType, LayerType inLayerType);
		CMediatedWindow*	FetchTopWindow(Int32 inWindType, LayerType inLayerType, Boolean inIncludeRestrictedTargets);
		CMediatedWindow*	FetchBottomWindow(Boolean inIncludeAlwaysLowered);
		
		void				CloseAllWindows(Int32 inWindType);
		
	protected:
	
		void				NoteWindowCreated(CMediatedWindow* inWindow);
		void				NoteWindowDisposed(CMediatedWindow* inWindow);		
		void				NoteWindowDescriptorChanged(CMediatedWindow* inWindow);
		void				NoteWindowActivated(CMediatedWindow* inWindow);
		void				NoteWindowDeactivated(CMediatedWindow* inWindow);
		void 				NoteWindowMenubarModeChanged(CMediatedWindow* inWindow);
		
		LArray				mWindowList;
		
		static CAutoPtr<CWindowMediator>	sMediator;

	private:
		friend class CAutoPtr<CWindowMediator>;
		
							CWindowMediator();
		virtual				~CWindowMediator();
};


