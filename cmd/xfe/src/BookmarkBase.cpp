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
/*---------------------------------------*/
/*																		*/
/* Name:		BookmarkBase.cpp										*/
/* Description:	XFE_BookmarkBase class source.							*/
/*				Base class for components dealing with bookmarks.		*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/* Date:		Tue Mar  4 03:45:16 PST 1997							*/


/*																		*/
/*----------------------------------------------------------------------*/

#include "BookmarkBase.h"
#include "MozillaApp.h"
#include "IconGroup.h"
#include "View.h"
#include "ToolbarDrop.h"
#include "PersonalToolbar.h"

#include "bkmks.h"
#include "felocale.h"
#include "intl_csi.h"

#include <Xfe/XfeAll.h>

#include <Xm/CascadeB.h>
#include <Xm/PushB.h>
#include <Xm/Separator.h>

#define MORE_BUTTON_NAME			"bookmarkMoreButton"
#define MORE_PULLDOWN_NAME			"bookmarkPulldown"
#define MENU_PANE_SEPARATOR_NAME	"menuPaneSeparator"
#define TOOL_BAR_SEPARATOR_NAME		"toolBarSeparator"

//////////////////////////////////////////////////////////////////////////
//
// Used to pass data to the callbacks
//
//////////////////////////////////////////////////////////////////////////
typedef struct _BookmarkCallbackStruct
{
	XFE_BookmarkBase *	object;		// The object
	BM_Entry *			entry;		// The bookmark entry
} BookmarkCallbackStruct;
//////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////
XFE_BookmarkBase::XFE_BookmarkBase(MWContext *	bookmarkContext,
								   XFE_Frame *	frame,
								   XP_Bool		onlyHeaders,
								   XP_Bool		fancyItems) :
	_bookmarkContext(bookmarkContext),
	_frame(frame),
	_onlyHeaders(onlyHeaders),
	_fancyItems(fancyItems),
	_charSetInfo(LO_GetDocumentCharacterSetInfo(_bookmarkContext)),
	_dropAddressBuffer(NULL),
	_dropTitleBuffer(NULL),
	_dropLastAccess(0)
{
	XP_ASSERT( _frame != NULL );

	// Ask MozillaApp to let us know when the bookmarks change
	XFE_MozillaApp::theApp()->registerInterest(
		XFE_MozillaApp::bookmarksHaveChanged,
		this,
		(XFE_FunctionNotification) bookmarksChanged_cb,
		this);

    XFE_MozillaApp::theApp()->registerInterest(
		XFE_MozillaApp::updateToolbarAppearance,
		this,
		(XFE_FunctionNotification)updateIconAppearance_cb);

	// the personal toolbar folder
    XFE_MozillaApp::theApp()->registerInterest(
		XFE_MozillaApp::personalToolbarFolderChanged,
		this,
		(XFE_FunctionNotification)updatePersonalToolbarFolderName_cb);

	createPixmaps();
}
//////////////////////////////////////////////////////////////////////////
/* virtual */
XFE_BookmarkBase::~XFE_BookmarkBase()
{
	XFE_MozillaApp::theApp()->unregisterInterest(
		XFE_MozillaApp::bookmarksHaveChanged,
		this,
		(XFE_FunctionNotification) bookmarksChanged_cb,
		this);

    XFE_MozillaApp::theApp()->unregisterInterest(
		XFE_MozillaApp::updateToolbarAppearance,
		this,
		(XFE_FunctionNotification)updateIconAppearance_cb);

	// the personal toolbar folder
    XFE_MozillaApp::theApp()->unregisterInterest(
		XFE_MozillaApp::personalToolbarFolderChanged,
		this,
		(XFE_FunctionNotification)updatePersonalToolbarFolderName_cb);

	if (_dropAddressBuffer)
	{
		XtFree(_dropAddressBuffer);
	}

	if (_dropTitleBuffer)
	{
		XtFree(_dropTitleBuffer);
	}
}
//////////////////////////////////////////////////////////////////////////

Pixmap XFE_BookmarkBase::_bookmarkPixmap = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_BookmarkBase::_bookmarkMask = XmUNSPECIFIED_PIXMAP;

Pixmap XFE_BookmarkBase::_bookmarkChangedPixmap = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_BookmarkBase::_bookmarkChangedMask = XmUNSPECIFIED_PIXMAP;

Pixmap XFE_BookmarkBase::_mailBookmarkPixmap = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_BookmarkBase::_mailBookmarkMask = XmUNSPECIFIED_PIXMAP;

Pixmap XFE_BookmarkBase::_newsBookmarkPixmap = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_BookmarkBase::_newsBookmarkMask = XmUNSPECIFIED_PIXMAP;

Pixmap XFE_BookmarkBase::_folderArmedPixmap = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_BookmarkBase::_folderArmedMask = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_BookmarkBase::_folderPixmap = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_BookmarkBase::_folderMask = XmUNSPECIFIED_PIXMAP;

Pixmap XFE_BookmarkBase::_personalFolderArmedMask = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_BookmarkBase::_personalFolderArmedPixmap = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_BookmarkBase::_personalFolderMask = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_BookmarkBase::_personalFolderPixmap = XmUNSPECIFIED_PIXMAP;

Pixmap XFE_BookmarkBase::_newFolderArmedMask = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_BookmarkBase::_newFolderArmedPixmap = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_BookmarkBase::_newFolderMask = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_BookmarkBase::_newFolderPixmap = XmUNSPECIFIED_PIXMAP;

Pixmap XFE_BookmarkBase::_menuFolderArmedMask = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_BookmarkBase::_menuFolderArmedPixmap = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_BookmarkBase::_menuFolderMask = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_BookmarkBase::_menuFolderPixmap = XmUNSPECIFIED_PIXMAP;

Pixmap XFE_BookmarkBase::_newMenuFolderArmedMask = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_BookmarkBase::_newMenuFolderArmedPixmap = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_BookmarkBase::_newMenuFolderMask = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_BookmarkBase::_newMenuFolderPixmap = XmUNSPECIFIED_PIXMAP;

Pixmap XFE_BookmarkBase::_newPersonalFolderArmedMask = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_BookmarkBase::_newPersonalFolderArmedPixmap = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_BookmarkBase::_newPersonalFolderMask = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_BookmarkBase::_newPersonalFolderPixmap = XmUNSPECIFIED_PIXMAP;

Pixmap XFE_BookmarkBase::_newPersonalMenuFolderArmedMask = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_BookmarkBase::_newPersonalMenuFolderArmedPixmap = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_BookmarkBase::_newPersonalMenuFolderMask = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_BookmarkBase::_newPersonalMenuFolderPixmap = XmUNSPECIFIED_PIXMAP;

//////////////////////////////////////////////////////////////////////////
void
XFE_BookmarkBase::createPixmaps()
{
	// Create icons
 	Pixel fg = XfeForeground(_frame->getBaseWidget());
 	Pixel bg = XfeBackground(_frame->getBaseWidget());

	if (!XfePixmapGood(XFE_BookmarkBase::_bookmarkPixmap))
	{
		IconGroup_createAllIcons(&BM_Bookmark_group,
								 _frame->getBaseWidget(),
								 fg,bg);

		_bookmarkPixmap = BM_Bookmark_group.pixmap_icon.pixmap;
		_bookmarkMask = BM_Bookmark_group.pixmap_icon.mask;
	}

	if (!XfePixmapGood(XFE_BookmarkBase::_bookmarkChangedPixmap))
	{
		IconGroup_createAllIcons(&BM_Change_group,
								 _frame->getBaseWidget(),
								 fg,bg);

		_bookmarkChangedPixmap = BM_Change_group.pixmap_icon.pixmap;
		_bookmarkChangedMask = BM_Change_group.pixmap_icon.mask;
	}

	if (!XfePixmapGood(XFE_BookmarkBase::_mailBookmarkPixmap))
	{
		IconGroup_createAllIcons(&BM_MailBookmark_group,
								 _frame->getBaseWidget(),
								 fg,bg);

		_mailBookmarkPixmap = BM_MailBookmark_group.pixmap_icon.pixmap;
		_mailBookmarkMask = BM_MailBookmark_group.pixmap_icon.mask;
	}

	if (!XfePixmapGood(XFE_BookmarkBase::_newsBookmarkPixmap))
	{
		IconGroup_createAllIcons(&BM_NewsBookmark_group,
								 _frame->getBaseWidget(),
								 fg,bg);

		_newsBookmarkPixmap = BM_NewsBookmark_group.pixmap_icon.pixmap;
		_newsBookmarkMask = BM_NewsBookmark_group.pixmap_icon.mask;
	}

	if (!XfePixmapGood(XFE_BookmarkBase::_folderPixmap))
	{
		IconGroup_createAllIcons(&BM_Folder_group,
								 _frame->getBaseWidget(),
								 fg,bg);

		_folderPixmap = BM_Folder_group.pixmap_icon.pixmap;
		_folderMask = BM_Folder_group.pixmap_icon.mask;
	}

	if (!XfePixmapGood(XFE_BookmarkBase::_folderArmedPixmap))
	{
		IconGroup_createAllIcons(&BM_FolderO_group,
								 _frame->getBaseWidget(),
								 fg,bg);

		_folderArmedPixmap = BM_FolderO_group.pixmap_icon.pixmap;
		_folderArmedMask = BM_FolderO_group.pixmap_icon.mask;
	}

	if (!XfePixmapGood(XFE_BookmarkBase::_personalFolderPixmap))
	{
		IconGroup_createAllIcons(&BM_PersonalFolder_group,
								 _frame->getBaseWidget(),
								 fg,bg);

		_personalFolderPixmap = BM_PersonalFolder_group.pixmap_icon.pixmap;
		_personalFolderMask = BM_PersonalFolder_group.pixmap_icon.mask;
	}

	if (!XfePixmapGood(XFE_BookmarkBase::_personalFolderArmedPixmap))
	{
		IconGroup_createAllIcons(&BM_PersonalFolderO_group,
								 _frame->getBaseWidget(),
								 fg,bg);

		_personalFolderArmedPixmap = BM_PersonalFolderO_group.pixmap_icon.pixmap;
		_personalFolderArmedMask = BM_PersonalFolderO_group.pixmap_icon.mask;
	}

	if (!XfePixmapGood(XFE_BookmarkBase::_newFolderPixmap))
	{
		IconGroup_createAllIcons(&BM_NewFolder_group,
								 _frame->getBaseWidget(),
								 fg,bg);

		_newFolderPixmap = BM_NewFolder_group.pixmap_icon.pixmap;
		_newFolderMask = BM_NewFolder_group.pixmap_icon.mask;
	}

	if (!XfePixmapGood(XFE_BookmarkBase::_newFolderArmedPixmap))
	{
		IconGroup_createAllIcons(&BM_NewFolderO_group,
								 _frame->getBaseWidget(),
								 fg,bg);

		_newFolderArmedPixmap = BM_NewFolderO_group.pixmap_icon.pixmap;
		_newFolderArmedMask = BM_NewFolderO_group.pixmap_icon.mask;
	}

	if (!XfePixmapGood(XFE_BookmarkBase::_menuFolderPixmap))
	{
		IconGroup_createAllIcons(&BM_MenuFolder_group,
								 _frame->getBaseWidget(),
								 fg,bg);

		_menuFolderPixmap = BM_MenuFolder_group.pixmap_icon.pixmap;
		_menuFolderMask = BM_MenuFolder_group.pixmap_icon.mask;
	}

	if (!XfePixmapGood(XFE_BookmarkBase::_menuFolderArmedPixmap))
	{
		IconGroup_createAllIcons(&BM_MenuFolderO_group,
								 _frame->getBaseWidget(),
								 fg,bg);

		_menuFolderArmedPixmap = BM_MenuFolderO_group.pixmap_icon.pixmap;
		_menuFolderArmedMask = BM_MenuFolderO_group.pixmap_icon.mask;
	}

	if (!XfePixmapGood(XFE_BookmarkBase::_newMenuFolderPixmap))
	{
		IconGroup_createAllIcons(&BM_NewAndMenuFolder_group,
								 _frame->getBaseWidget(),
								 fg,bg);

		_newMenuFolderPixmap = BM_NewAndMenuFolder_group.pixmap_icon.pixmap;
		_newMenuFolderMask = BM_NewAndMenuFolder_group.pixmap_icon.mask;
	}

	if (!XfePixmapGood(XFE_BookmarkBase::_newMenuFolderArmedPixmap))
	{
		IconGroup_createAllIcons(&BM_NewAndMenuFolderO_group,
								 _frame->getBaseWidget(),
								 fg,bg);

		_newMenuFolderArmedPixmap = BM_NewAndMenuFolderO_group.pixmap_icon.pixmap;
		_newMenuFolderArmedMask = BM_NewAndMenuFolderO_group.pixmap_icon.mask;
	}

	if (!XfePixmapGood(XFE_BookmarkBase::_newPersonalFolderPixmap))
	{
		IconGroup_createAllIcons(&BM_NewPersonalFolder_group,
								 _frame->getBaseWidget(),
								 fg,bg);

		_newPersonalFolderPixmap = BM_NewPersonalFolder_group.pixmap_icon.pixmap;
		_newPersonalFolderMask = BM_NewPersonalFolder_group.pixmap_icon.mask;
	}

	if (!XfePixmapGood(XFE_BookmarkBase::_newPersonalFolderArmedPixmap))
	{
		IconGroup_createAllIcons(&BM_NewPersonalFolderO_group,
								 _frame->getBaseWidget(),
								 fg,bg);

		_newPersonalFolderArmedPixmap = BM_NewPersonalFolderO_group.pixmap_icon.pixmap;
		_newPersonalFolderArmedMask = BM_NewPersonalFolderO_group.pixmap_icon.mask;
	}

	if (!XfePixmapGood(XFE_BookmarkBase::_newPersonalMenuFolderPixmap))
	{
		IconGroup_createAllIcons(&BM_NewPersonalMenu_group,
								 _frame->getBaseWidget(),
								 fg,bg);

		_newPersonalMenuFolderPixmap = BM_NewPersonalMenu_group.pixmap_icon.pixmap;
		_newPersonalMenuFolderMask = BM_NewPersonalMenu_group.pixmap_icon.mask;
	}

	if (!XfePixmapGood(XFE_BookmarkBase::_newPersonalMenuFolderArmedPixmap))
	{
		IconGroup_createAllIcons(&BM_NewPersonalMenuO_group,
								 _frame->getBaseWidget(),
								 fg,bg);

		_newPersonalMenuFolderArmedPixmap = BM_NewPersonalMenuO_group.pixmap_icon.pixmap;
		_newPersonalMenuFolderArmedMask = BM_NewPersonalMenuO_group.pixmap_icon.mask;
	}
}
//////////////////////////////////////////////////////////////////////////

/* static */ void
XFE_BookmarkBase::item_armed_cb(Widget		w,
								XtPointer	clientData,
								XtPointer	/* call_data */)
{
	BookmarkCallbackStruct *	data = (BookmarkCallbackStruct *) clientData;
	XFE_BookmarkBase *			object = data->object;
	BM_Entry *					entry = data->entry;

	object->entryArmed(w,entry);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_BookmarkBase::item_enter_cb(Widget		w,
								XtPointer	clientData,
								XtPointer	/* call_data */)
{
	BookmarkCallbackStruct *	data = (BookmarkCallbackStruct *) clientData;
	XFE_BookmarkBase *			object = data->object;
	BM_Entry *					entry = data->entry;

	object->entryEnter(w,entry);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_BookmarkBase::item_leave_cb(Widget	w,
								  XtPointer	clientData,
								  XtPointer	/* call_data */)
{
	BookmarkCallbackStruct *	data = (BookmarkCallbackStruct *) clientData;
	XFE_BookmarkBase *			object = data->object;
	BM_Entry *					entry = data->entry;

	object->entryLeave(w,entry);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_BookmarkBase::item_disarmed_cb(Widget		w,
								   XtPointer	clientData,
								   XtPointer	/* call_data */)
{
	BookmarkCallbackStruct *	data = (BookmarkCallbackStruct *) clientData;
	XFE_BookmarkBase *			object = data->object;
	BM_Entry *					entry = data->entry;

	object->entryDisarmed(w,entry);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_BookmarkBase::item_activated_cb(Widget		w,
									XtPointer	clientData,
									XtPointer	/* call_data */)
{
	BookmarkCallbackStruct *	data = (BookmarkCallbackStruct *) clientData;
	XFE_BookmarkBase *			object = data->object;
	BM_Entry *					entry = data->entry;

	if (XfeBmAccentIsEnabled() && (XfeIsBmButton(w) || XfeIsBmCascade(w)))
	{
		unsigned char accent_type;
		
		XtVaGetValues(w,XmNaccentType,&accent_type,NULL);

		// Add the new entry above the activated bookmark item
		if (accent_type == XmACCENT_TOP)
		{
			object->addEntryBefore(object->getDropAddress(),
								   object->getDropTitle(),
								   object->getDropLastAccess(),
								   entry);
		}
		// Add the new entry below the activated bookmark item
		else if (accent_type == XmACCENT_BOTTOM)
		{
			object->addEntryAfter(object->getDropAddress(),
								  object->getDropTitle(),
								  object->getDropLastAccess(),
								  entry);
		}
		// Add the new entry into the activated bookmark folder
		else if (accent_type == XmACCENT_ALL)
		{
			object->addEntryToFolder(object->getDropAddress(),
									 object->getDropTitle(),
									 object->getDropLastAccess(),
									 entry);
		}
		
		// Now disable dropping
		object->disableDropping();
	}
	// A fancy cascade button was activated
	else if (XmIsCascadeButton(w) && entry && BM_IsHeader(entry))
	{
		XFE_Frame *		frame = object->getFrame();
		MWContext *		context = frame->getContext();
		History_entry * he = SHIST_GetCurrent(&context->hist);

		char *			address;
		char *			title;
		BM_Date			lastAccess;

		XP_ASSERT( frame != NULL );
		XP_ASSERT( context != NULL );
		XP_ASSERT( he != NULL );

		address = he->address;
		
		title = (he->title && *he->title) ? he->title : address;

		lastAccess = he->last_access;

		object->addEntryToFolder(address,title,lastAccess,entry);
	}
	// A push button was activated
	else
	{
		object->entryActivated(w,entry);
	}
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_BookmarkBase::item_cascading_cb(Widget		cascade,
									XtPointer	clientData,
									XtPointer	/* call_data */)
{
	BookmarkCallbackStruct *	data = (BookmarkCallbackStruct *) clientData;
	XFE_BookmarkBase *			object = data->object;
	BM_Entry *					entry = data->entry;
	Widget						submenu = NULL;

	XtVaGetValues(cascade,XmNsubMenuId,&submenu,NULL);

	if (entry && submenu) 
	{
		XP_ASSERT(BM_IsHeader(entry));

		// Add the first it in a "File Bookmark" hierarchy.  This should
		// only happen if _onlyHeaders is true and the current bookmark
		// header has at least one item that is also a header.
		if (object->_onlyHeaders)
		{
			Widget file_here = object->createCascadeButton(submenu,entry,True);
			Widget sep = object->createSeparator(submenu);

			XtManageChild(file_here);
			XtManageChild(sep);
		}

		object->createItemTree(submenu,BM_GetChildren(entry));
	}

	XtRemoveCallback(cascade,
					 XmNcascadingCallback,
					 &XFE_BookmarkBase::item_cascading_cb,
					 (XtPointer) data);

	object->entryCascading(cascade,entry);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_BookmarkBase::item_free_data_cb(Widget		/* item */,
									XtPointer	clientData,
									XtPointer	/* call_data */)
{
	BookmarkCallbackStruct *	data = (BookmarkCallbackStruct *) clientData;

	XP_FREE(data);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_BookmarkBase::pane_mapping_eh(Widget		submenu,
								  XtPointer		clientData,
								  XEvent *		event,
								  Boolean *		cont)
{
 	XFE_BookmarkBase * object = (XFE_BookmarkBase *) clientData;

	// Make sure the widget is alive and this is an unmap event
	if (!XfeIsAlive(submenu) || !event || (event->type != UnmapNotify))
	{
		return;
	}

	// Look for the cascade ancestor
	Widget cascade = 
		XfeAncestorFindByClass(submenu,xfeCascadeWidgetClass,XfeFIND_ANY);
			   
	if (cascade)
	{
		Boolean armed;

		// Determine if the cascade is armed
		XtVaGetValues(cascade,XmNarmed,&armed,NULL);

		// If it is armed, then it means the menu hierarchy is still 
		// visible and a shared submenu as the one that caused this 
		// event.  Those motif shared sumenu shells are very complicated.

		if (armed)
		{
			return;
		}

		object->disableDropping();
	}

	*cont = True;
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_BookmarkBase, bookmarksChanged)
	(XFE_NotificationCenter *	/* obj */,
	 void *						/* clientData */,
	 void *						/* call_data */)
{
	// Update the bookmarks since they have changed
	prepareToUpdateRoot();
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_BookmarkBase,updateIconAppearance)
	(XFE_NotificationCenter *	/*obj*/, 
	 void *						/*clientData*/, 
	 void *						/*callData*/)
{
	// Update the appearance
	updateAppearance();
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_BookmarkBase,updatePersonalToolbarFolderName)
	(XFE_NotificationCenter *	/* obj */, 
	 void *						/* clientData */, 
	 void *						/* callData */)
{
	updateToolbarFolderName();
}
//////////////////////////////////////////////////////////////////////////
XFE_Frame *
XFE_BookmarkBase::getFrame()
{
	return _frame;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BookmarkBase::getPixmapsForEntry(BM_Entry *	entry,
									 Pixmap *	pixmapOut,
									 Pixmap *	maskOut,
									 Pixmap *	armedPixmapOut,
									 Pixmap *	armedMaskOut)
{
	Pixmap pixmap		= _bookmarkPixmap;
	Pixmap mask			= _bookmarkMask;
	Pixmap armedPixmap	= XmUNSPECIFIED_PIXMAP;
	Pixmap armedMask	= XmUNSPECIFIED_PIXMAP;

	// Right now the only way an entry can be NULL os for the 
	// bookmarkMoreButton which kinda looks and acts like a folder so
	// we use folder pixmaps for it
	if (!entry)
	{
		pixmap		= _folderPixmap;
		mask		= _folderMask;
		armedPixmap	= _folderArmedPixmap;
		armedMask	= _folderArmedMask;
	}
	else
	{
		if (BM_IsHeader(entry))
		{
			XP_Bool is_tool = (entry == XFE_PersonalToolbar::getToolbarFolder());
			XP_Bool is_menu = (entry == getMenuFolder());
			XP_Bool is_add = (entry == getAddFolder());
			
			if (is_add && is_menu && is_tool)
			{
				pixmap		= _newPersonalMenuFolderPixmap;
				mask		= _newPersonalMenuFolderMask;
				armedPixmap	= _newPersonalMenuFolderArmedPixmap;
				armedMask	= _newPersonalMenuFolderArmedMask;
			}
			else if (is_add && is_tool)
			{
				pixmap		= _newPersonalFolderPixmap;
				mask		= _newPersonalFolderMask;
				armedPixmap	= _newPersonalFolderArmedPixmap;
				armedMask	= _newPersonalFolderArmedMask;
			}
			else if (is_add && is_menu)
			{
				pixmap		= _newMenuFolderPixmap;
				mask		= _newMenuFolderMask;
				armedPixmap	= _newMenuFolderArmedPixmap;
				armedMask	= _newMenuFolderArmedMask;
			}
			else if (is_add)
			{
				pixmap		= _newFolderPixmap;
				mask		= _newFolderMask;
				armedPixmap	= _newFolderArmedPixmap;
				armedMask	= _newFolderArmedMask;
			}
			else if (is_menu)
			{
				pixmap		= _menuFolderPixmap;
				mask		= _menuFolderMask;
				armedPixmap	= _menuFolderArmedPixmap;
				armedMask	= _menuFolderArmedMask;
			}
			else if (is_tool)
			{
				pixmap		= _personalFolderPixmap;
				mask		= _personalFolderMask;
				armedPixmap	= _personalFolderArmedPixmap;
				armedMask	= _personalFolderArmedMask;
			}
			else
			{
				pixmap		= _folderPixmap;
				mask		= _folderMask;
				armedPixmap	= _folderArmedPixmap;
				armedMask	= _folderArmedMask;
			}
		}
		else
		{
			int url_type = NET_URL_Type(BM_GetAddress(entry));
			
			switch (url_type)
			{
			case IMAP_TYPE_URL:
			case MAILBOX_TYPE_URL:
				
				pixmap	= _mailBookmarkPixmap;
				mask	= _mailBookmarkMask;
				
				break;
				
			case NEWS_TYPE_URL:
				
				pixmap	= _newsBookmarkPixmap;
				mask	= _newsBookmarkMask;
				
				break;
				
			default:

				if( BM_GetChangedState(entry) == BM_CHANGED_YES )
				{
					pixmap  = _bookmarkChangedPixmap;
					mask    = _bookmarkChangedMask;
				}			
				break;
			}
		}
	}

	if (pixmapOut)
	{
		*pixmapOut = pixmap;
	}

	if (maskOut)
	{
		*maskOut = mask;
	}

	if (armedPixmapOut)
	{
		*armedPixmapOut = armedPixmap;
	}

	if (armedMaskOut)
	{
		*armedMaskOut = armedMask;
	}

}
//////////////////////////////////////////////////////////////////////////
BM_Entry *
XFE_BookmarkBase::getFirstEntry()
{
	// Access the root header
	BM_Entry * root = BM_GetMenuHeader(_bookmarkContext);

	// Ignore the root header (ie, "Joe's Bookmarks")
	if (root && BM_IsHeader(root))
	{
		root = BM_GetChildren(root);
	}

	return root;
}
//////////////////////////////////////////////////////////////////////////
BM_Entry *
XFE_BookmarkBase::getTopLevelFolder(const char * name)
{
	BM_Entry * result = NULL;

	// Access the root header
	BM_Entry * root = getRootFolder();

	// Ignore the root header (ie, "Joe's Bookmarks")
	if (root && BM_IsHeader(root))
	{
		BM_Entry * entry = BM_GetChildren(root);

		while (entry && !result)
		{
			if (BM_IsHeader(entry))
			{
				if (XP_STRCMP(name,BM_GetName(entry)) == 0)
				{
					result = entry;
				}
			}

			entry = BM_GetNext(entry);
		}
	}

	return result;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BookmarkBase::trackSubmenuMapping(Widget submenu)
{
	XP_ASSERT( XfeIsAlive(submenu) );
	XP_ASSERT( XmIsRowColumn(submenu) );

	Widget shell = XtParent(submenu);

	XP_ASSERT( shell );

	XtAddEventHandler(shell,
					  StructureNotifyMask,
					  True,
					  &XFE_BookmarkBase::pane_mapping_eh,
					  (XtPointer) this);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BookmarkBase::createItemTree(Widget menu,BM_Entry * entry)
{
	while (entry)
	{
		Widget item = NULL;

		if (BM_IsHeader(entry))
		{
			item = createCascadeButton(menu,entry,False);
		}
		else if (!_onlyHeaders)
		{
			if (BM_IsSeparator(entry))
			{
				item = createSeparator(menu);
			}
			else
			{
				item = createPushButton(menu,entry);
			}
		}

		if (item)
		{
			XP_ASSERT( XfeIsAlive(item) );

			XtManageChild(item);
		}

		entry = BM_GetNext(entry);
	}
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_BookmarkBase::prepareToUpdateRoot()
{
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_BookmarkBase::reallyUpdateRoot()
{
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_BookmarkBase::updateAppearance()
{
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_BookmarkBase::updateToolbarFolderName()
{
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_BookmarkBase::entryArmed(Widget /* button */,BM_Entry * entry)
{
	char * address;

	if (!entry || !(BM_IsAlias(entry) || BM_IsUrl(entry)))
	{
		return;
	}
	
	address = BM_GetAddress(entry);
	
	_frame->notifyInterested(XFE_View::statusNeedsUpdatingMidTruncated,
							 (void*) address);
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_BookmarkBase::entryEnter(Widget button,BM_Entry * entry)
{
	// Same thing as armed
	entryArmed(button,entry);
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_BookmarkBase::entryLeave(Widget button,BM_Entry * entry)
{
	// Same thing as disarmed
	entryDisarmed(button,entry);
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_BookmarkBase::entryDisarmed(Widget /* button */,BM_Entry * entry)
{
	if (!entry || !(BM_IsAlias(entry) || BM_IsUrl(entry)))
	{
		return;
	}
	
	_frame->notifyInterested(XFE_View::statusNeedsUpdatingMidTruncated,
							 (void*) "");
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_BookmarkBase::entryActivated(Widget /*item*/,BM_Entry * entry)
{
	LO_AnchorData anchor_data;
	
	if (!entry || !(BM_IsAlias(entry) || BM_IsUrl(entry)))
	{
		return;
	}

	anchor_data.anchor = (PA_Block)BM_GetAddress(entry);
	anchor_data.target = (PA_Block)BM_GetTarget(entry, TRUE);

	// Have the frame do the command if it supports it
	if (_frame->handlesCommand(xfeCmdOpenTargetUrl,(void *) &anchor_data))
	{
		_frame->doCommand(xfeCmdOpenTargetUrl,(void*) &anchor_data);
	}
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_BookmarkBase::entryCascading(Widget /* cascade */,BM_Entry * /* entry */)
{
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_BookmarkBase::enableDropping()
{
	// Enable the drawing of accents on the menu hierarchy
	XfeBmAccentSetFileMode(XmACCENT_FILE_ANYWHERE);
	XfeBmAccentEnable();
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_BookmarkBase::disableDropping()
{
	// Disable the drawing of accents on the menu hierarchy
	XfeBmAccentDisable();
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ XP_Bool
XFE_BookmarkBase::isDroppingEnabled()
{
	return XfeBmAccentIsEnabled();
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Menu component creation functions
//
//////////////////////////////////////////////////////////////////////////
Widget 
XFE_BookmarkBase::createCascadeButton(Widget		menu,
									  BM_Entry *	entry,
									  XP_Bool		ignore_children)
{
	XP_ASSERT( XfeIsAlive(menu) );
	XP_ASSERT( XmIsRowColumn(menu) );

	Widget						non_full_menu;
	Widget						cascade;
	Widget						submenu = NULL;
	Arg							av[20];
	Cardinal					ac;
	XmString					xmname;
	BookmarkCallbackStruct *	data = NULL;
	XP_Bool						has_header_children;

	xmname = XFE_BookmarkBase::entryToXmString(_bookmarkContext,
											   entry,
											   _charSetInfo);

	non_full_menu = getLastMoreMenu(menu);

	XP_ASSERT( XfeIsAlive(non_full_menu) );

	// Determne if we have at least one header child
	has_header_children = XFE_BookmarkBase::BM_EntryHasHeaderChildren(entry);

	// Create sub menu only if needed (children exits and not only eaders)
	if (BM_GetChildren(entry) && 
		!(_onlyHeaders && !has_header_children) &&
		!ignore_children)
	{
		Visual *	visual = XfeVisual(non_full_menu);
		Colormap	cmap = XfeColormap(non_full_menu);
		Cardinal	depth = XfeDepth(non_full_menu);
		
		ac = 0;
		XtSetArg(av[ac],XmNvisual,		visual);	ac++;
		XtSetArg(av[ac],XmNdepth,		depth);		ac++;
		XtSetArg(av[ac],XmNcolormap,	cmap);		ac++;

  		submenu = XmCreatePulldownMenu(non_full_menu,"bookmarkPulldown",av,ac);
	}

	ac = 0;
	XtSetArg(av[ac],XmNlabelString,	xmname);	ac++;

	if (XfeIsAlive(submenu))
	{
		XtSetArg(av[ac],XmNsubMenuId,	submenu);	ac++;
	}

	if (_fancyItems)
	{
 		cascade = XfeCreateBmCascade(non_full_menu,"bookmarkCascadeButton",av,ac);

		configureXfeBmCascade(cascade,entry);
	}
	else
	{
		cascade = XmCreateCascadeButton(non_full_menu,"bookmarkCascadeButton",av,ac);

		configureCascade(cascade,entry);
	}

	// Create a new bookmark data structure for the callbacks
	data = XP_NEW_ZAP(BookmarkCallbackStruct);

	data->object	= this;
	data->entry		= entry;

	XtAddCallback(cascade,
				  XmNcascadingCallback,
				  &XFE_BookmarkBase::item_cascading_cb,
				  (XtPointer) data);

	XtAddCallback(cascade,
				  XmNactivateCallback,
				  &XFE_BookmarkBase::item_activated_cb,
				  (XtPointer) data);

	XtAddCallback(cascade,
				  XmNdestroyCallback,
				  &XFE_BookmarkBase::item_free_data_cb,
				  (XtPointer) data);

	if (xmname)
	{
		XmStringFree(xmname);
	}

	return cascade;
}
//////////////////////////////////////////////////////////////////////////
Widget 
XFE_BookmarkBase::createMoreButton(Widget menu)
{
	XP_ASSERT( XfeIsAlive(menu) );
	XP_ASSERT( XmIsRowColumn(menu) );

	Widget		cascade;
	Widget		submenu = NULL;
	Arg			av[20];
	Cardinal	ac = 0;

	ac = 0;
	XtSetArg(av[ac],XmNvisual,		XfeVisual(menu));	ac++;
	XtSetArg(av[ac],XmNdepth,		XfeDepth(menu));	ac++;
	XtSetArg(av[ac],XmNcolormap,	XfeColormap(menu));	ac++;
	
	submenu = XmCreatePulldownMenu(menu,MORE_PULLDOWN_NAME,av,ac);

	ac = 0;
	XtSetArg(av[ac],XmNsubMenuId,	submenu);	ac++;

	if (_fancyItems)
	{
 		cascade = XfeCreateBmCascade(menu,MORE_BUTTON_NAME,av,ac);

		configureXfeBmCascade(cascade,NULL);
	}
	else
	{
		cascade = XmCreateCascadeButton(menu,MORE_BUTTON_NAME,av,ac);

		configureCascade(cascade,NULL);
	}

	return cascade;
}
//////////////////////////////////////////////////////////////////////////
Widget 
XFE_BookmarkBase::createPushButton(Widget menu,BM_Entry * entry)
{
	XP_ASSERT( XfeIsAlive(menu) );
	XP_ASSERT( XmIsRowColumn(menu) );

	Widget						non_full_menu;
	Widget						button;
	Arg							av[20];
	Cardinal					ac;
	XmString					xmname;
	BookmarkCallbackStruct *	data = NULL;

	xmname = XFE_BookmarkBase::entryToXmString(_bookmarkContext,
											   entry,
											   _charSetInfo);

	ac = 0;
	XtSetArg(av[ac], XmNlabelString,	xmname);	ac++;

	non_full_menu = getLastMoreMenu(menu);

	XP_ASSERT( XfeIsAlive(non_full_menu) );

	// must be xfeCmdOpenTargetUrl or sensitivity becomes a problem
	if (_fancyItems)
	{
		button = XfeCreateBmButton(non_full_menu,xfeCmdOpenTargetUrl,av,ac);

		configureXfeBmButton(button,entry);
	}
	else
	{
		button = XmCreatePushButton(non_full_menu,xfeCmdOpenTargetUrl,av,ac);

		configureButton(button,entry);
	}

	// Create a new bookmark data structure for the callbacks
	data = XP_NEW_ZAP(BookmarkCallbackStruct);

	data->object	= this;
	data->entry		= entry;
	
	XtAddCallback(button,
				  XmNactivateCallback,
				  &XFE_BookmarkBase::item_activated_cb,
				  (XtPointer) data);

	XtAddCallback(button,
				  XmNarmCallback,
				  &XFE_BookmarkBase::item_armed_cb,
				  (XtPointer) data);

	XtAddCallback(button,
				  XmNdisarmCallback,
				  &XFE_BookmarkBase::item_disarmed_cb,
				  (XtPointer) data);

	XtAddCallback(button,
				  XmNdestroyCallback,
				  &XFE_BookmarkBase::item_free_data_cb,
				  (XtPointer) data);

	if (xmname)
	{
		XmStringFree(xmname);
	}

	return button;
}
//////////////////////////////////////////////////////////////////////////
Widget 
XFE_BookmarkBase::getLastMoreMenu(Widget menu)
{
	XP_ASSERT( XfeIsAlive(menu) );
	XP_ASSERT( XmIsRowColumn(menu) );

	// Find the last more... menu
	Widget last_more_menu = XfeMenuFindLastMoreMenu(menu,MORE_BUTTON_NAME);

	XP_ASSERT( XfeIsAlive(last_more_menu) );

	// Check if the last menu is full
	if (XfeMenuIsFull(last_more_menu))
	{
		// Look for the More... button for the last menu
		Widget more_button = XfeMenuGetMoreButton(last_more_menu,
												  MORE_BUTTON_NAME);

		// If no more button, create one plus a submenu
		if (!more_button)
		{
			more_button = createMoreButton(last_more_menu);

			XtManageChild(more_button);

		}

		// Set the last more menu to the submenu of the new more button
		last_more_menu = XfeCascadeGetSubMenu(more_button);
	}

	return last_more_menu;
}
//////////////////////////////////////////////////////////////////////////
Widget 
XFE_BookmarkBase::createSeparator(Widget menu)
{
	XP_ASSERT( XfeIsAlive(menu) );

	Widget		parent;		
	Widget		separator;
	String		name;

	if (XmIsRowColumn(menu))
	{
		parent = getLastMoreMenu(menu);

		name = MENU_PANE_SEPARATOR_NAME;
	}
	else
	{
		parent = menu;

		name = TOOL_BAR_SEPARATOR_NAME;
	}

	XP_ASSERT( XfeIsAlive(parent) );

	separator = XmCreateSeparator(parent,name,NULL,0);

	configureSeparator(separator,NULL);

	XtManageChild(separator);
	
	return separator;
}
//////////////////////////////////////////////////////////////////////////
Widget 
XFE_BookmarkBase::createXfeCascade(Widget parent,BM_Entry * entry)
{
	XP_ASSERT( XfeIsAlive(parent) );

	Widget						cascade;
	Widget						submenu;
	Arg							av[20];
	Cardinal					ac;
	XmString					xmname;
	BookmarkCallbackStruct *	data = NULL;

	xmname = XFE_BookmarkBase::entryToXmString(_bookmarkContext,
											   entry,
											   _charSetInfo);

	// No submenu needed if the entry has no children
// 	if (!BM_GetChildren(entry))
// 	{
// 		// fix me
// 	}

	ac = 0;
	XtSetArg(av[ac],XmNlabelString,		xmname);				ac++;
	XtSetArg(av[ac],XmNuserData,		entry);					ac++;

	cascade = XfeCreateCascade(parent,"bookmarkCascade",av,ac);

	configureXfeCascade(cascade,entry);

	// Create a new bookmark data structure for the callbacks
	data = XP_NEW_ZAP(BookmarkCallbackStruct);

	data->object	= this;
	data->entry		= entry;

	XtAddCallback(cascade,
				  XmNcascadingCallback,
				  &XFE_BookmarkBase::item_cascading_cb,
				  (XtPointer) data);

	XtAddCallback(cascade,
				  XmNdestroyCallback,
				  &XFE_BookmarkBase::item_free_data_cb,
				  (XtPointer) data);

	if (xmname)
	{
		XmStringFree(xmname);
	}

	// Obtain the cascade's submenu
	XtVaGetValues(cascade,XmNsubMenuId,&submenu,NULL);

	// Keep track of the submenu mapping
	trackSubmenuMapping(submenu);

	return cascade;
}
//////////////////////////////////////////////////////////////////////////
Widget 
XFE_BookmarkBase::createXfeButton(Widget parent,BM_Entry * entry)
{
	XP_ASSERT( XfeIsAlive(parent) );

	Widget						button;
	Arg							av[20];
	Cardinal					ac;
	XmString					xmname;
	BookmarkCallbackStruct *	data = NULL;

	xmname = XFE_BookmarkBase::entryToXmString(_bookmarkContext,
											   entry,
											   _charSetInfo);

	ac = 0;
	XtSetArg(av[ac],XmNlabelString,		xmname);					ac++;
	XtSetArg(av[ac],XmNuserData,		entry);						ac++;

	button = XfeCreateButton(parent,"bookmarkButton",av,ac);

	configureXfeButton(button,entry);

	// Create a new bookmark data structure for the callbacks
	data = XP_NEW_ZAP(BookmarkCallbackStruct);

	data->object	= this;
	data->entry		= entry;

	XtAddCallback(button,
				  XmNactivateCallback,
				  &XFE_BookmarkBase::item_activated_cb,
				  (XtPointer) data);

	XtAddCallback(button,
				  XmNarmCallback,
				  &XFE_BookmarkBase::item_armed_cb,
				  (XtPointer) data);

	XtAddCallback(button,
				  XmNdisarmCallback,
				  &XFE_BookmarkBase::item_disarmed_cb,
				  (XtPointer) data);

	XtAddCallback(button,
				  XmNenterCallback,
				  &XFE_BookmarkBase::item_enter_cb,
				  (XtPointer) data);

	XtAddCallback(button,
				  XmNleaveCallback,
				  &XFE_BookmarkBase::item_leave_cb,
				  (XtPointer) data);

	XtAddCallback(button,
				  XmNdestroyCallback,
				  &XFE_BookmarkBase::item_free_data_cb,
				  (XtPointer) data);

	if (xmname)
	{
		XmStringFree(xmname);
	}

	return button;
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_BookmarkBase::configureXfeCascade(Widget /*item*/,BM_Entry * /*entry*/)
{
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_BookmarkBase::configureXfeButton(Widget /*item*/,BM_Entry * /*entry*/)
{
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_BookmarkBase::configureXfeBmButton(Widget item,BM_Entry * entry)
{
	if (fe_globalPrefs.toolbar_style == BROWSER_TOOLBAR_TEXT_ONLY)
	{
		XtVaSetValues(item,
					  XmNlabelPixmap,		XmUNSPECIFIED_PIXMAP,
					  XmNlabelPixmapMask,	XmUNSPECIFIED_PIXMAP,
					  NULL);

		return;
	}

	Pixmap pixmap;
	Pixmap mask;

	getPixmapsForEntry(entry,&pixmap,&mask,NULL,NULL);

	XtVaSetValues(item,
				  XmNlabelPixmap,		pixmap,
				  XmNlabelPixmapMask,	mask,
				  NULL);
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_BookmarkBase::configureXfeBmCascade(Widget item,BM_Entry * entry)
{
	if (fe_globalPrefs.toolbar_style == BROWSER_TOOLBAR_TEXT_ONLY)
	{
		XtVaSetValues(item,
					  XmNlabelPixmap,		XmUNSPECIFIED_PIXMAP,
					  XmNlabelPixmapMask,	XmUNSPECIFIED_PIXMAP,
					  XmNarmPixmap,			XmUNSPECIFIED_PIXMAP,
					  XmNarmPixmapMask,		XmUNSPECIFIED_PIXMAP,
					  NULL);

		return;
	}

	Pixmap pixmap;
	Pixmap mask;
	Pixmap armedPixmap;
	Pixmap armedMask;

	getPixmapsForEntry(entry,&pixmap,&mask,&armedPixmap,&armedMask);

	Arg			av[4];
	Cardinal	ac = 0;

	XtSetArg(av[ac],XmNlabelPixmap,		pixmap); ac++;
	XtSetArg(av[ac],XmNlabelPixmapMask,	mask); ac++;

	// Only show the aremd pixmap/mask if this entry has children
	if (XfeIsAlive(XfeCascadeGetSubMenu(item)))
	{
		XtSetArg(av[ac],XmNarmPixmap,		armedPixmap); ac++;
		XtSetArg(av[ac],XmNarmPixmapMask,	armedMask); ac++;
	}

	XtSetValues(item,av,ac);
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_BookmarkBase::configureButton(Widget /*item*/,BM_Entry * /*entry*/)
{
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_BookmarkBase::configureCascade(Widget /*item*/,BM_Entry * /*entry*/)
{
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_BookmarkBase::configureSeparator(Widget /*item*/,BM_Entry * /*entry*/)
{
}
//////////////////////////////////////////////////////////////////////////
MWContext *
XFE_BookmarkBase::getBookmarkContext()
{
	return _bookmarkContext;
}
//////////////////////////////////////////////////////////////////////////
BM_Entry *
XFE_BookmarkBase::getRootFolder()
{
	return BM_GetRoot(getBookmarkContext());
//	return BM_GetAddHeader(getBookmarkContext());
}
//////////////////////////////////////////////////////////////////////////
BM_Entry *
XFE_BookmarkBase::getAddFolder()
{
	return BM_GetAddHeader(getBookmarkContext());
}
//////////////////////////////////////////////////////////////////////////
BM_Entry *
XFE_BookmarkBase::getMenuFolder()
{
	return BM_GetMenuHeader(getBookmarkContext());
}
//////////////////////////////////////////////////////////////////////////
/* static */ BM_Entry *
XFE_BookmarkBase::BM_FindFolderByName(BM_Entry * root,char * folder_name)
{
	if (!root || !folder_name)
	{
		return NULL;
	}

	BM_Entry * header;

	char * header_name = BM_GetName(root);

	if(XP_STRCMP(header_name,folder_name) == 0)
	{
		return root;
	}

	for (BM_Entry * bm = BM_GetChildren(root); 
		 bm != NULL; 
		 bm = BM_GetNext(bm))
	{
		if (BM_IsHeader(bm))
		{
			header = BM_FindFolderByName(bm,folder_name);

			if(header != NULL)
			{
				return header;
			}
		}
	}

	return NULL;
}
//////////////////////////////////////////////////////////////////////////
/* static */ BM_Entry *
XFE_BookmarkBase::BM_FindEntryByAddress(BM_Entry *		root,
										const char *	target_address)
{
	if (!root || !target_address)
	{
		return NULL;
	}

	char * entry_address = BM_GetAddress(root);

	if (entry_address && (XP_STRCMP(target_address,entry_address) == 0))
	{
		return root;
	}

	if (BM_IsHeader(root))
	{
		BM_Entry * entry;

		for (entry = BM_GetChildren(root); entry; entry = BM_GetNext(entry))
		{
			BM_Entry * test_entry;

			test_entry = 
				XFE_BookmarkBase::BM_FindEntryByAddress(entry,target_address);
			
			if (test_entry)
			{
				return test_entry;
			}
		}
	}

	return NULL;
}
//////////////////////////////////////////////////////////////////////////
/* static */ BM_Entry *
XFE_BookmarkBase::BM_FindNextEntry(BM_Entry * current)
{
	if (current)
	{
		return BM_GetNext(current);
 	}
	
 	return NULL;
}
//////////////////////////////////////////////////////////////////////////
/* static */ BM_Entry *
XFE_BookmarkBase::BM_FindPreviousEntry(BM_Entry * current)
{

	if (current)
	{
		BM_Entry * parent = BM_GetParent(current);

		if (parent && BM_IsHeader(parent))
		{
			BM_Entry * entry;
			BM_Entry * prev = NULL;

			for (entry = BM_GetChildren(parent); 
				 entry;
				 entry = BM_GetNext(entry))
			{
				if (entry == current)
				{
					return prev;
				}

				prev = entry;
			}
		}
	}

	return NULL;
}
//////////////////////////////////////////////////////////////////////////
/* static */ XP_Bool
XFE_BookmarkBase::BM_EntryHasHeaderChildren(BM_Entry * header)
{
	XP_ASSERT( header != NULL );
	XP_ASSERT( BM_IsHeader(header) );

	BM_Entry * entry = BM_GetChildren(header);

	// Traverse until we find a header entry
	while (entry)
	{
		if (BM_IsHeader(entry))
		{
			return True;
		}

		entry = BM_GetNext(entry);
	}

	return False;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BookmarkBase::addEntryAfter(const char *	address,
								const char *	title,
								BM_Date			lastAccess,
								BM_Entry *		entryAfter)
{
	XP_ASSERT( entryAfter != NULL );

	// Create the new entry
	BM_Entry * entry = BM_NewUrl(title,address,NULL,lastAccess);

	// Add it after the given entry
	BM_InsertItemAfter(getBookmarkContext(),entryAfter,entry);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BookmarkBase::addEntryBefore(const char *	address,
								 const char *	title,
								 BM_Date		lastAccess,
								 BM_Entry *		entryBefore)
{
	XP_ASSERT( entryBefore != NULL );

	// In order to add a bookmark before and entry, we try to add it after
	// the previous entry if it exists or prepend it to the parent header
	// if it does not.  The latter case will occur ehtn entryBefore is the
	// first entry in a list.
	BM_Entry * prev = XFE_BookmarkBase::BM_FindPreviousEntry(entryBefore);

	// Create the new entry
	BM_Entry * entry = BM_NewUrl(title,address,NULL,lastAccess);

	if (prev)
	{
		// Add it after the previous entry
		BM_InsertItemAfter(getBookmarkContext(),prev,entry);
	}
	else
	{
		// Try to access the parent
		BM_Entry * parent = BM_GetParent(entryBefore);

		// Use the root entry if no parent is found
		if (!parent)
		{
			parent = getRootFolder();
		}

		// Add the entry to the head of the list
		BM_PrependChildToHeader(getBookmarkContext(),parent,entry);
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BookmarkBase::addEntryToFolder(const char *		address,
								   const char *		title,
								   BM_Date			lastAccess,
								   BM_Entry *		header)
{
	XP_ASSERT( header != NULL );
	XP_ASSERT( BM_IsHeader(header) );

	BM_Entry * entry = BM_NewUrl(title,address,NULL,lastAccess);

	BM_AppendToHeader(getBookmarkContext(),header,entry);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BookmarkBase::setDropAddress(const char * address)
{
	if (_dropAddressBuffer)
	{
		XtFree(_dropAddressBuffer);
	}

	_dropAddressBuffer = NULL;

	if (address)
	{
		_dropAddressBuffer = (char *) XtNewString(address);
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BookmarkBase::setDropTitle(const char * title)
{
	if (_dropTitleBuffer)
	{
		XtFree(_dropTitleBuffer);
	}

	_dropTitleBuffer = NULL;

	if (title)
	{
		_dropTitleBuffer = (char *) XtNewString(title);
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_BookmarkBase::setDropLastAccess(BM_Date lastAccess)
{
	_dropLastAccess = lastAccess;
}
//////////////////////////////////////////////////////////////////////////
const char *
XFE_BookmarkBase::getDropAddress()
{
	return _dropAddressBuffer;
}
//////////////////////////////////////////////////////////////////////////
const char *
XFE_BookmarkBase::getDropTitle()
{
	return _dropTitleBuffer;
}
//////////////////////////////////////////////////////////////////////////
BM_Date
XFE_BookmarkBase::getDropLastAccess()
{
	return _dropLastAccess;
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_BookmarkBase::guessTitle(XFE_Frame *	frame,
							 MWContext *	bookmarkContext,
							 const char *	address,
							 XP_Bool		sameShell,
							 char **		guessedTitleOut,
							 BM_Date *		guessedLastDateOut)
{
	XP_ASSERT( frame != NULL );
	XP_ASSERT( bookmarkContext != NULL );
	XP_ASSERT( address != NULL );
	XP_ASSERT( guessedTitleOut != NULL );
	XP_ASSERT( guessedLastDateOut != NULL );
	
	// Initialize the results to begin with
	*guessedLastDateOut	= 0;
	*guessedTitleOut		= NULL;

	// Obtain the frame's context
	MWContext *	frame_context = frame->getContext();

	// If the operation occurs in the same shell, look in the history.
	if (sameShell)
	{
		// Obtain the current history entry
		History_entry * hist_entry = SHIST_GetCurrent(&frame_context->hist);

		// Make sure the history entry matches the given address, or else
		// the caller used a the same shell with a link that is not the 
		// the proxy icon (ie, in the html area).
		if (hist_entry && (XP_STRCMP(hist_entry->address,address) == 0))
		{
			*guessedTitleOut = hist_entry->title;
			*guessedLastDateOut = hist_entry->last_access;
		}
	}

	// Look in the bookmarks for a dup
	if (!*guessedTitleOut)
	{
		BM_Entry * test_entry = NULL;
		BM_Entry * root_entry = BM_GetRoot(bookmarkContext);

		if (root_entry)
		{
			test_entry = XFE_BookmarkBase::BM_FindEntryByAddress(root_entry,
																 address);
		}

		if (test_entry)
		{
			*guessedTitleOut = BM_GetName(test_entry);
		}
	}

	// As a last resort use the address itself as a title
	if (!*guessedTitleOut)
	{
		*guessedTitleOut = (char *) address;
	}

	// Strip the leading http:// from the title
	if (XP_STRLEN(*guessedTitleOut) > 7)
	{
		if ((XP_STRNCMP(*guessedTitleOut,"http://",7) == 0) ||
			(XP_STRNCMP(*guessedTitleOut,"file://",7) == 0))
		{
			(*guessedTitleOut) += 7;
		}
	}
}
//////////////////////////////////////////////////////////////////////////

/* static */ XmString
XFE_BookmarkBase::entryToXmString(MWContext *		bookmarkContext,
								  BM_Entry *		entry,
								  INTL_CharSetInfo	char_set_info)
{
	XP_ASSERT( entry != NULL );
	XP_ASSERT( bookmarkContext != NULL );

	XmString			result = NULL;
	XmString			tmp;
	char *				psz;

	tmp = XFE_BookmarkBase::formatItem(bookmarkContext,
									   entry,
									   True,
									   INTL_DefaultWinCharSetID(NULL));
	
	// Mid truncate the name
	if (XmStringGetLtoR(tmp,XmSTRING_DEFAULT_CHARSET,&psz))
	{

		XmStringFree(tmp);

		INTL_MidTruncateString(INTL_GetCSIWinCSID(char_set_info),psz,psz,40);

		result = XmStringCreateLtoR(psz,XmSTRING_DEFAULT_CHARSET);

		if (psz)
		{
			XtFree(psz);
		}
	}
	else
	{
		result = tmp;
	}

	return result;
}
//////////////////////////////////////////////////////////////////////////
/* static */ XmString
XFE_BookmarkBase::formatItem(MWContext *	bookmarkContext, 
							 BM_Entry *		entry, 
							 Boolean		no_indent,
							 int16			charset)
{
  XmString xmhead, xmtail, xmcombo;
  int depth = (no_indent ? 0 : BM_GetDepth(bookmarkContext, entry));
  char head [255];
  char buf [1024];
  int j = 0;
  char *title = BM_GetName(entry);
  char *url = BM_GetAddress(entry);
  int indent = 2;
  int left_offset = 2;
  XmFontList font_list;

  if (! no_indent)
    {
      head [j++] = (BM_IsHeader(entry)
		    ? (BM_IsFolded(entry) ? '+' : '-') :
		    BM_IsSeparator(entry) ? ' ' :
		    /* ### item->last_visit == 0*/ 0 ? '?' : ' ');
      while (j < ((depth * indent) + left_offset))
	head [j++] = ' ';
    }
  head [j] = 0;

  if (BM_IsSeparator(entry))
    {
      strcpy (buf, "-------------------------");
    }
  else if (title || url)
    {
      fe_FormatDocTitle (title, url, buf, 1024);
    }
  else if (BM_IsUrl(entry))
    {
#if 0
      strcpy (buf, XP_GetString( XFE_GG_EMPTY_LL ) );
#endif
    }

  if (!*head)
    xmhead = 0;
  else
    xmhead = XmStringSegmentCreate (head, "ICON", XmSTRING_DIRECTION_L_TO_R,
				    False);

  if (!*buf)
    xmtail = 0;
  else if (BM_IsUrl(entry))
    xmtail = fe_ConvertToXmString ((unsigned char *) buf, charset, 
										NULL, XmFONT_IS_FONT, &font_list);
  else
    {
      char *loc;

      loc = (char *) fe_ConvertToLocaleEncoding (charset,
							(unsigned char *) buf);
      xmtail = XmStringSegmentCreate (loc, "HEADING",
					XmSTRING_DIRECTION_L_TO_R, False);
      if (loc != buf)
	{
	  XP_FREE(loc);
	}
    }

  if (xmhead && xmtail)
    {
      int size = XmStringLength (xmtail);

      /*
	 There is a bug down under XmStringNConcat() where, in the process of
	 appending the two strings, it will read up to four bytes past the end
	 of the second string.  It doesn't do anything with the data it reads
	 from there, but people have been regularly crashing as a result of it,
	 because sometimes these strings end up at the end of the page!!

	 I tried making the second string a bit longer (by adding some spaces
	 to the end of `buf') and passing in a correspondingly smaller length
	 to XmStringNConcat(), but that causes it not to append *any* of the
	 characters in the second string.  So XmStringNConcat() would seem to
	 simply not work at all when the length isn't exactly the same as the
	 length of the second string.

	 Plan B is to simply realloc() the XmString to make it a bit larger,
	 so that we know that it's safe to go off the end.

	 The NULLs at the beginning of the data we append probably aren't
	 necessary, but they are read and parsed as a length, even though the
	 data is not used (with the current structure of the XmStringNConcat()
	 code.)  (The initialization of this memory is only necessary at all
	 to keep Purify from (rightly) squawking about it.)

	 And oh, by the way, the strncpy() down there, which I have verified
	 is doing strncpy( <the-right-place> , <the-string-below>, 15 )
	 actually writes 15 \000 bytes instead of four \000 bytes plus the
	 random text below.  So I seem to have stumbled across a bug in
	 strncpy() as well, even though it doesn't hose me.  THIS TIME. -- jwz
       */
#define MOTIF_BUG_BUFFER "\000\000\000\000 MOTIF BUG"
#ifdef MOTIF_BUG_BUFFER
      xmtail = (XmString) realloc ((void *) xmtail,
				   size + sizeof (MOTIF_BUG_BUFFER));
      strncpy (((char *) xmtail) + size, MOTIF_BUG_BUFFER,
	       sizeof (MOTIF_BUG_BUFFER));
# undef MOTIF_BUG_BUFFER
#endif /* MOTIF_BUG_BUFFER */

      xmcombo = XmStringNConcat (xmhead, xmtail, size);
    }
  else if (xmhead)
    {
      xmcombo = xmhead;
      xmhead = 0;
    }
  else if (xmtail)
    {
      xmcombo = xmtail;
      xmtail = 0;
    }
  else
    {
      xmcombo = XmStringCreateLtoR ("", XmFONTLIST_DEFAULT_TAG);
    }

  if (xmhead) XmStringFree (xmhead);
  if (xmtail) XmStringFree (xmtail);
  return (xmcombo);
}
//////////////////////////////////////////////////////////////////////////
