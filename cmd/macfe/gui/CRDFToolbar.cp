/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "CRDFToolbar.h"

#include <cassert>
#include "htrdf.h"



static
SPaneInfo
pane_params_from( HT_View /*ht_view*/, LView* pp_superview )
	{
		SPaneInfo info;

		info.paneID			= 0;

		SDimension16 superview_size;
		pp_superview->GetFrameSize(superview_size);
		info.width			= superview_size.width;

		info.height			= 24; // NO! Get this value from the |HT_View|.
		info.visible		= true;
		info.enabled		= true;

		SBooleanRect bindings = { true, true, true, false };
		info.bindings	= bindings;

		info.left				= 0;
		info.top				= 0;

		info.userCon		= 0;
		info.superView	= pp_superview;

		return info;
	}

static
SViewInfo
view_params_from( HT_View /*ht_view*/ )
	{
		SViewInfo info;

		SDimension32 image_size = { 0, 0 };
		info.imageSize = image_size;

		SPoint32 scroll_pos = { 0, 0 };
		info.scrollPos = scroll_pos;

		SPoint32 scroll_unit = { 1, 1 };
		info.scrollUnit = scroll_unit;

		info.reconcileOverhang = 0;

		return info;
	}

static
bool
is_docked( HT_View /*ht_view*/ )
	{
		return false;
	}



CRDFToolbar::CRDFToolbar( HT_View ht_view, LView* pp_superview )
		:	CDragBar( pane_params_from(ht_view, pp_superview), view_params_from(ht_view), is_docked(ht_view) ),
			_ht_view(ht_view)
	{
		assert( _ht_view );											// There must be an |HT_View|...
		assert( !HT_GetViewFEData(_ht_view) );	//	...and it must not be linked to any other FE object.

		HT_SetViewFEData(_ht_view, this);

			// TO BE FIXED: 1103 needs a better name and visibility
			// TO BE FIXED: is there a better way to insert the grippy pane?

		LWindow* window = LWindow::FetchWindowObject(pp_superview->GetMacPort());
		UReanimator::CreateView(1103, this, window);	// create the CPatternedGrippyPane
#if 0
		LView* view = UReanimator::CreateView(1104, this, window);	// create the CToolbarPatternBevelView
		view->ResizeFrameBy(-12, 0, false);
#endif
	}

CRDFToolbar::~CRDFToolbar()
	{
		assert( _ht_view );											// There must be an |HT_View|...

		HT_SetViewFEData(_ht_view, 0);
	}

void
CRDFToolbar::HandleNotification( HT_Notification, HT_Resource, HT_Event, void*, uint32 )
	{
		// ...
	}
