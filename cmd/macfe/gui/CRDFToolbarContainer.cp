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

#include "CRDFToolbarContainer.h"
#include "CRDFToolbar.h"


const ClassIDT CRDFToolbarContainer::class_ID;



CRDFToolbarContainer::CRDFToolbarContainer( LStream* inStream )
		: CDragBarContainer(inStream)
	{
		/*
			We don't want to actually create the toolbar |HT_Pane| until everything else exists,
			i.e., after we've done everything needed in |FinishCreateSelf()|.
		*/
	}


void
CRDFToolbarContainer::FinishCreateSelf()
	{
		CDragBarContainer::FinishCreateSelf();
		
			// Everything we need to create the |HT_Pane| now exists...
		_ht_root = auto_ptr<_HT_PaneStruct>(HT_NewToolbarPane(CreateNotificationStruct()));
		HT_SetPaneFEData(_ht_root.get(), this);
	}


void
CRDFToolbarContainer::BuildToolbarsPresentAtStartup()
	{
	}


void 
CRDFToolbarContainer::RestorePlace( LStream *inPlace )
 {
 }


void
CRDFToolbarContainer::HandleNotification( HT_Notification notification, HT_Resource node, HT_Event event, void* token, uint32 tokenType )
	{
		HT_View ht_view = HT_GetView(node);

		switch ( event )
			{
				case HT_EVENT_VIEW_ADDED:		// i.e., create a toolbar
					AddBar( new CRDFToolbar(ht_view, this) );
					break;

				case HT_EVENT_VIEW_DELETED:	// i.e., destroy a toolbar
					if( CRDFToolbar* toolbar = reinterpret_cast<CRDFToolbar*>(HT_GetViewFEData(ht_view)) )
						delete toolbar;
					break;

#if 0
				case HT_EVENT_NODE_ADDED:
				case HT_EVENT_NODE_DELETED_DATA:
				case HT_EVENT_NODE_DELETED_NODATA:
				case HT_EVENT_NODE_VPROP_CHANGED:
				case HT_EVENT_NODE_SELECTION_CHANGED:
				case HT_EVENT_NODE_OPENCLOSE_CHANGED:
				case HT_EVENT_VIEW_SELECTED:
				case HT_EVENT_NODE_OPENCLOSE_CHANGING:
				case HT_EVENT_VIEW_SORTING_CHANGED:
				case HT_EVENT_VIEW_REFRESH:
				case HT_EVENT_VIEW_WORKSPACE_REFRESH:
				case HT_EVENT_NODE_EDIT:
				case HT_EVENT_WORKSPACE_EDIT:
				case HT_EVENT_VIEW_HTML_ADD:
				case HT_EVENT_VIEW_HTML_REMOVE:
				case HT_EVENT_NODE_ENABLE:
				case HT_EVENT_NODE_DISABLE:
				case HT_EVENT_NODE_SCROLLTO:
				case HT_EVENT_COLUMN_ADD:
				case HT_EVENT_COLUMN_DELETE:
				case HT_EVENT_COLUMN_SIZETO:
				case HT_EVENT_COLUMN_REORDER:
				case HT_EVENT_COLUMN_SHOW:
				case HT_EVENT_COLUMN_HIDE:
				case HT_EVENT_VIEW_MODECHANGED:
#endif
				default:	// If it's not a message for me, it may be for some object under me...
					if ( CRDFToolbar* toolbar = reinterpret_cast<CRDFToolbar*>(HT_GetViewFEData(ht_view)) )
						toolbar->HandleNotification(notification, node, event, token, tokenType);
			}
	}
