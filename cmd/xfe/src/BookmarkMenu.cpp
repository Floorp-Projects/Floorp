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
/* 
   BookmarkMenu.cpp -- class for doing the dynamic bookmark menus
   Created: Chris Toshok <toshok@netscape.com>, 19-Dec-1996.
 */



#include "BookmarkMenu.h"
#include "BookmarkFrame.h"
#include "BookmarkView.h"
#include "PersonalToolbar.h"
#include "bkmks.h"

#include <Xfe/XfeAll.h>

#define IS_CASCADE(w)	(XmIsCascadeButton(w) || XmIsCascadeButtonGadget(w))
#define IS_PUSH(w)		(XmIsPushButton(w) || XmIsPushButtonGadget(w))

//////////////////////////////////////////////////////////////////////////
XFE_BookmarkMenu::XFE_BookmarkMenu(MWContext *	bookmarkContext,
								   Widget		cascade,
								   XFE_Frame *	frame,
								   XP_Bool		onlyHeaders,
								   XP_Bool		fancyItems) :
	XFE_BookmarkBase(bookmarkContext,frame,onlyHeaders,fancyItems),
	_cascade(cascade),
	_subMenu(NULL),
	_firstSlot(0)
{
	// Obtain the submenu and the first available slot
	XtVaGetValues(_cascade,XmNsubMenuId,&_subMenu,NULL);
	XtVaGetValues(_subMenu,XmNnumChildren,&_firstSlot,NULL);

	// When the cascade is blown away, so are we
	XtAddCallback(_cascade,
				  XmNdestroyCallback,
				  &XFE_BookmarkMenu::destroy_cb,
				  (XtPointer) this);

	// make sure we initially install an update callback
 	XtAddCallback(_cascade,
				  XmNcascadingCallback,
				  &XFE_BookmarkMenu::update_cb,
				  (XtPointer) this);

	// Keep track of the submenu mapping
	trackSubmenuMapping(_subMenu);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BookmarkMenu::generate(Widget		cascade, 
						   XtPointer	clientData,
						   XFE_Frame *	frame)
{
	XFE_BookmarkMenu * object;

	object = new XFE_BookmarkMenu(XFE_BookmarkFrame::main_bm_context,
								  cascade,
								  frame,
								  (int) clientData,
								  True);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_BookmarkMenu::generateQuickfile(Widget		cascade, 
									XtPointer	clientData,
									XFE_Frame *	frame)
{
	XFE_BookmarkMenu * object;

	object = new XFE_BookmarkMenu(XFE_BookmarkFrame::main_bm_context,
								  cascade,
								  frame,
								  (int) clientData,
								  True);

	// Store the BookmarkMenu instance in the quickfile button 
	// XmNinstancePointer.  This overrides the XmNinstancePointer 
	// installed by the XFE_Button class
	XtVaSetValues(cascade,XmNinstancePointer,(XtPointer) object,NULL);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_BookmarkMenu::destroy_cb(Widget		/* w */,
							 XtPointer	client_data,
							 XtPointer	/* call_data*/)
{
	XFE_BookmarkMenu * object = (XFE_BookmarkMenu *) client_data;

	delete object;
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_BookmarkMenu::update_cb(Widget		cascade,
							XtPointer	client_data,
							XtPointer	/* call_data */)
{
	XFE_BookmarkMenu *	object = (XFE_BookmarkMenu *) client_data;
	Widget				subMenu;

	XtVaGetValues(cascade,XmNsubMenuId,&subMenu,NULL);

	// Really update
	object->reallyUpdateRoot();

	// Make sure the submenu is realized
	XtRealizeWidget(subMenu);

	// Remove this callback now that we have been updated
	XtRemoveCallback(cascade,
					 XmNcascadingCallback,
					 &XFE_BookmarkMenu::update_cb,
					 (XtPointer) object);
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_BookmarkMenu::prepareToUpdateRoot()
{
	// This may seem stupid, but it keeps us from having more than
	// one reference to this particular callback without having
	// to worry about other cascadingCallbacks.
	
	// remove it if it's already there
	XtRemoveCallback(_cascade,
					 XmNcascadingCallback,
					 &XFE_BookmarkMenu::update_cb,
					 (XtPointer) this);
	
	// and then add it back.
	XtAddCallback(_cascade,
				  XmNcascadingCallback,
				  &XFE_BookmarkMenu::update_cb,
				  (XtPointer) this);
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_BookmarkMenu::reallyUpdateRoot()
{
 	WidgetList		children;
 	Cardinal		numChildren;
 	BM_Entry *		root = getMenuFolder();

	// Ignore the root header (ie, "Joe's Bookmarks")
	if (root && BM_IsHeader(root))
	{
		root = BM_GetChildren(root);
	}

 	XfeChildrenGet(_subMenu,&children,&numChildren);	
	
	//  XtUnrealizeWidget(m_subMenu);

 	// Get rid of the previous items we created
 	if (children && numChildren)
	{
 		children += _firstSlot;

 		numChildren -= _firstSlot;

 		if (children && numChildren)
		{
 			XtUnmanageChildren(children,numChildren);
      
 			fe_DestroyWidgetTree(children,numChildren);
		}
	}

 	// Create the entries if any
 	if (root)
 	{
 		createItemTree(_subMenu,root);
 	}
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_BookmarkMenu::enableDropping()
{
	// Gurantee that the popup and items and created and realized or
	// else the setFixedSensitive() call will have no items to modify
	update_cb(_cascade,(XtPointer) this,(XtPointer) NULL);

	// Chain
	XFE_BookmarkBase::enableDropping();

	// Make all the fixed items insensitive
	setFixedItemSensitive(False);
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_BookmarkMenu::disableDropping()
{
	// Chain
	XFE_BookmarkBase::disableDropping();

	// Make all the fixed items sensitive
	setFixedItemSensitive(True);
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_BookmarkMenu::enableFiling()
{
	XfeBmAccentSetFileMode(XmACCENT_FILE_SELF);
	XfeBmAccentEnable();
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_BookmarkMenu::disableFiling()
{
	XfeBmAccentSetFileMode(XmACCENT_FILE_ANYWHERE);
	XfeBmAccentDisable();
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BookmarkMenu::setFixedItemSensitive(XP_Bool state)
{
 	WidgetList		children;
 	Cardinal		numChildren;

 	XfeChildrenGet(_subMenu,&children,&numChildren);	
	
 	// Make sure some fixed items exist
 	if (children && numChildren && _firstSlot && (_firstSlot < numChildren))
	{
		Cardinal i;

		// Set the sensitivity state for all the fixed push button items
		for (i = 0; i < _firstSlot; i++)
		{
			if (IS_PUSH(children[i]) || IS_CASCADE(children[i]))
			{
				XtSetSensitive(children[i],state);
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////
