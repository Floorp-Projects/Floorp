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

// CToolbarDragBar.cp

#include <LArrayIterator.h>
#include <LControl.h>

#include "CToolbarDragBar.h"
#include "CToolbarModeManager.h"
#include "CButton.h"

CToolbarDragBar::CToolbarDragBar(LStream* inStream)
	: CDragBar(inStream)
{
}

CToolbarDragBar::~CToolbarDragBar()
{
}

void CToolbarDragBar::FinishCreateSelf()
{
	// if current toolbar mode is different that defaultToolbarMode
	// then we need to change mode of this drag bar
	Int32 toolbarStyle;
	int result = CToolbarModeManager::GetToolbarPref(toolbarStyle);
	if (result == noErr)
	{
	//	97-10-07 pchen -- Don't check against default toolbar mode; we still
	//	need to call HandleModeChange because cobrand large icon might
	//	make "spinning N" button bigger than size in PPob.
	//	if (toolbarStyle != CToolbarModeManager::defaultToolbarMode)
			HandleModeChange(toolbarStyle);
	}
}

void CToolbarDragBar::HandleModeChange(Int8 inNewMode)
{
	SDimension16 sizeChange;
	LArrayIterator	iter(mSubPanes);
	LPane*			pane = NULL;

	while (iter.Next(&pane))
	{
		CToolbarButtonContainer* container =
			dynamic_cast<CToolbarButtonContainer*>(pane);
		if (container)
		{
			container->HandleModeChange(inNewMode, sizeChange);
		}
	}
	
	// check to see if we need to change our height
	if (sizeChange.height)
	{
		// Now resize drag bar based on heightChange
		ResizeFrameBy(0, sizeChange.height, false);
		
		// broadcast msg_DragBarCollapse with a null ioParam
		// to force container to readjust itself because we've changed
		// out size
		BroadcastMessage(msg_DragBarCollapse);
	}
}
