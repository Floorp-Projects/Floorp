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

// CToolbarModeManager.h

#pragma once

#include <LBroadcaster.h>
#include <LListener.h>

//#include "resgui.h"	// for TOOLBAR_TEXT_AND_ICONS #define

enum {
	eTOOLBAR_ICONS,					// Toolbar display style possible values
	eTOOLBAR_TEXT,
	eTOOLBAR_TEXT_AND_ICONS
};

class CToolbarPrefsProxy;

class CToolbarModeManager
{
	public:
		enum {	msg_ToolbarModeChange = 'modC'	};

		// defaultToolbarMode is mode that PPob's are defined for
		enum {	defaultToolbarMode = eTOOLBAR_TEXT_AND_ICONS };
		
//		static void			BroadcastToolbarModeChange();
		static int			BroadcastToolbarModeChange(
								const char* domain,
								void* instance_data);
		static void			AddToolbarModeListener(LListener* inListener);

		static void			SetPrefsProxy(CToolbarPrefsProxy* inPrefsProxy)
							{	sPrefsProxy = inPrefsProxy;	}

		static int			GetToolbarPref(Int32& ioPref);

	protected:
		static LBroadcaster*		sBroadcaster;
		static CToolbarPrefsProxy*	sPrefsProxy;
};

// Mix-in classes to get toolbar mode change propagated to the
// toolbar buttons

// This is the class that listens for changes in toolbar mode
// and "handles" mode change
class CToolbarModeChangeListener
	:	public LListener
{
	public:
							CToolbarModeChangeListener();

		virtual void		ListenToMessage(MessageT inMessage, void* ioParam);
	
	protected:
		// Pure virtual. Subclasses must implement
		virtual void		HandleModeChange(Int8 inNewMode) = 0;
};

// This is the class that has the toolbar buttons as shallow subpanes
class CToolbarButtonContainer
{
	public:
							CToolbarButtonContainer() {}
		virtual				~CToolbarButtonContainer() {}

		// Pure virtual. Subclasses must implement
		virtual void		HandleModeChange(Int8 inNewMode, SDimension16& outSizeChange) = 0;
};
