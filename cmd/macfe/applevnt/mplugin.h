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

#pragma once

#include "np.h"
#include "npapi.h"

#include <Printing.h>
#include <Icons.h>

#include <LView.h>
#include <LPeriodical.h>
#include <LCommander.h>
#include <LDragAndDrop.h>
#include <TArray.h>

class StPluginFocus;

//
// Return the "plug-in descriptor" that we pass to XP code
// (this is really just a pointer to a CPluginHandler).
//
class CStr255;
extern void* GetPluginDesc(const CStr255& pluginName);


/*********************************************************************************
 * CPluginView
 * Embeds the plugin, and passes on the events to it
 *********************************************************************************/

class CPluginView : public LView,		// Drawing, etc
					public LPeriodical,	// Idling
					public LCommander,	// Key events
					public LDragAndDrop	// Dragging
{
public:
	friend class StPluginFocus;
	enum { class_ID = 'plug' };

// ¥¥ constructors
							CPluginView(LStream *inStream);
	virtual					~CPluginView();

			void			EmbedCreate(MWContext* context, LO_EmbedStruct* embed_struct);
			void			EmbedFree(MWContext* context, LO_EmbedStruct* embed_struct);
			void			EmbedSize(LO_EmbedStruct* embed_struct, SDimension16 hyperSize);
			void			EmbedDisplay(LO_EmbedStruct* embed_struct, Boolean isPrinting);

// ¥¥ access
			NPWindow*		GetNPWindow() 						{ return &fNPWindow; }
			NPEmbeddedApp*	GetNPEmbeddedApp()					{ return fApp; }

// ¥¥ event processing
	static	CPluginView*	sPluginTarget;
	static	void			BroadcastPluginEvent(const EventRecord& event);
	static	Boolean			PluginWindowEvent(const EventRecord& event);
	
	virtual void			ClickSelf(const SMouseDownEvent& inMouseDown);
	virtual void			EventMouseUp(const EventRecord &inMacEvent);
	virtual Boolean			ObeyCommand(CommandT inCommand, void *ioParam);
	virtual Boolean			HandleKeyPress(const EventRecord& inKeyEvent);
	virtual void			DrawSelf();
	virtual void			SpendTime(const EventRecord& inMacEvent);
	virtual void			ActivateSelf();
	virtual void			DeactivateSelf();
	virtual void			BeTarget();
	virtual void			DontBeTarget();
	virtual void			AdjustCursorSelf(Point inPortPt, const EventRecord& inMacEvent);
			Boolean			PassEvent(EventRecord& inEvent);
	virtual Boolean			HandleEmbedEvent(CL_Event *event);
	
// ¥¥ positioning
	virtual void			AdaptToNewSurroundings();
	virtual void			AdaptToSuperFrameSize(Int32 inSurrWidthDelta, Int32 inSurrHeightDelta, Boolean inRefresh);
	virtual void			MoveBy(Int32 inHorizDelta, Int32 inVertDelta, Boolean inRefresh);
			Boolean			IsPositioned() const { return fPositioned; }
// ¥¥Êdragging
	virtual Boolean			DragIsAcceptable(DragReference inDragRef);
	virtual void			HiliteDropArea(DragReference inDragRef);
	virtual void			UnhiliteDropArea(DragReference inDragRef);

// ¥¥ printing
			Boolean			PrintFullScreen(Boolean printOne, THPrint printRecHandle);
			void			PrintEmbedded();
			
// ¥¥ broken plugin
			void			SetBrokenPlugin();
			void			DrawBroken(Boolean hilite);

			void			SetPositioned(Boolean positioned = true) { fPositioned = positioned; }

// ¥¥ window control

			void 			RegisterWindow(void* window);
			void 			UnregisterWindow(void* window);
			Boolean			PassWindowEvent(EventRecord& inEvent, WindowPtr window);
			
			SInt16			AllocateMenuID(Boolean isSubmenu);
			Boolean			IsPluginCommand(CommandT inCommand);
			
	static	CPluginView*	FindPlugin(WindowPtr window);
	
private:
			void			ResetDrawRect();
	
			NPEmbeddedApp*	fApp;
			NPWindow		fNPWindow;
			NP_Port			fNPPort;
			CPluginView*	fOriginalView;
			CIconHandle		fBrokenIcon;
			short			fHorizontalOffset;
			short			fVerticalOffset;
			Boolean			fBrokenPlugin;
			Boolean			fPositioned;
			Boolean			fHidden;
			Boolean			fWindowed;
			LO_EmbedStruct* fEmbedStruct;
			Boolean			fIsPrinting;
			LArray*			fWindowList;
			TArray<SInt16>*	fMenuList;
			
	static	LArray*			sPluginList;
};

