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
/* Name:		BookmarkBase.h											*/
/* Description:	XFE_BookmarkBase class header.							*/
/*				Base class for components dealing with bookmarks.		*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/* Date:		Tue Mar  4 03:45:16 PST 1997							*/


/*																		*/
/*----------------------------------------------------------------------*/

#include "NotificationCenter.h"
#include "Frame.h"
#include "intl_csi.h"
#include "bkmks.h"

#ifndef _xfe_bookmark_base_h_
#define _xfe_bookmark_base_h_

// This class can be used with a DYNA_CASCADEBUTTON or
// DYNA_MENUITEMS.

class XFE_BookmarkBase : public XFE_NotificationCenter
{

public:

 	XFE_BookmarkBase			(MWContext *	bookmarkContext,
								 XFE_Frame *	frame,
 								 XP_Bool		onlyHeaders,
								 XP_Bool		fancyItems);

	virtual ~XFE_BookmarkBase	();

	MWContext *			getBookmarkContext();

	static BM_Entry * BM_FindFolderByName	(BM_Entry *		root_entry,
											 char *			folder_name);

	static BM_Entry * BM_FindEntryByAddress(BM_Entry *		root_entry,
										 const char *	entry_name);

	static BM_Entry * BM_FindNextEntry		(BM_Entry *		entry);
	static BM_Entry * BM_FindPreviousEntry	(BM_Entry *		entry);

	static XP_Bool	BM_EntryHasHeaderChildren	(BM_Entry *		header);

	static void		guessTitle			(XFE_Frame *	frame,
										 MWContext *	bookmarkContext,
										 const char *	address,
										 XP_Bool		sameShell,
										 char **		resolvedTitleOut,
										 BM_Date *		resolvedLastDateOut);
	

	void			addEntryAfter		(const char *	address,
										 const char *	title,
										 BM_Date		lastAccess,
										 BM_Entry *		entryAfter);
	
	void			addEntryBefore		(const char *	address,
										 const char *	title,
										 BM_Date		lastAccess,
										 BM_Entry *		entryBefore);
	
	void			addEntryToFolder	(const char *	address,
										 const char *	title,
										 BM_Date		lastAccess,
										 BM_Entry *		header);

	void			setDropAddress		(const char *	address);
	void			setDropTitle		(const char *	title);
	void			setDropLastAccess	(BM_Date		lastAccess);

	const char *	getDropAddress		();
	const char *	getDropTitle		();
	BM_Date			getDropLastAccess	();

	// Access methods
	XFE_Frame *		getFrame			();

	virtual void	enableDropping			();
	virtual void	disableDropping			();
	virtual XP_Bool	isDroppingEnabled		();

protected:

	static Pixmap _bookmarkPixmap;
	static Pixmap _bookmarkMask;

	static Pixmap _bookmarkChangedPixmap;
	static Pixmap _bookmarkChangedMask;

	static Pixmap _mailBookmarkPixmap;
	static Pixmap _mailBookmarkMask;

	static Pixmap _newsBookmarkPixmap;
	static Pixmap _newsBookmarkMask;

	static Pixmap _folderArmedPixmap;
	static Pixmap _folderArmedMask;
	static Pixmap _folderPixmap;
	static Pixmap _folderMask;

	static Pixmap _personalFolderArmedMask;
	static Pixmap _personalFolderArmedPixmap;
	static Pixmap _personalFolderMask;
	static Pixmap _personalFolderPixmap;

	static Pixmap _newFolderArmedMask;
	static Pixmap _newFolderArmedPixmap;
	static Pixmap _newFolderMask;
	static Pixmap _newFolderPixmap;

	static Pixmap _menuFolderArmedMask;
	static Pixmap _menuFolderArmedPixmap;
	static Pixmap _menuFolderMask;
	static Pixmap _menuFolderPixmap;

	static Pixmap _newMenuFolderArmedMask;
	static Pixmap _newMenuFolderArmedPixmap;
	static Pixmap _newMenuFolderMask;
	static Pixmap _newMenuFolderPixmap;

	static Pixmap _newPersonalFolderArmedMask;
	static Pixmap _newPersonalFolderArmedPixmap;
	static Pixmap _newPersonalFolderMask;
	static Pixmap _newPersonalFolderPixmap;

	static Pixmap _newPersonalMenuFolderArmedMask;
	static Pixmap _newPersonalMenuFolderArmedPixmap;
	static Pixmap _newPersonalMenuFolderMask;
	static Pixmap _newPersonalMenuFolderPixmap;


	void getPixmapsForEntry(BM_Entry *	entry,
							Pixmap *	pixmap,
							Pixmap *	mask,
							Pixmap *	armedPixmap,
							Pixmap *	armedMask);

#if 0
	/* 	static */ Pixmap bookmarkPixmapFromEntry(BM_Entry *entry);
 	/* static */ Pixmap bookmarkMaskFromEntry(BM_Entry *entry);
#endif

	static void getBookmarkPixmaps(Pixmap & pixmap_out,Pixmap & mask_out);

	// Access bookmark entries
	BM_Entry *		getFirstEntry			();
	BM_Entry *		getTopLevelFolder		(const char * name);

	// Create a bookmark item menu tree
	void			createItemTree			(Widget menu,BM_Entry * entry);

	// Install submenu pane that controls dropping disability
	void			trackSubmenuMapping		(Widget submenu);

	// Override in derived class to configure
	virtual void	entryArmed				(Widget,BM_Entry *);
	virtual void	entryDisarmed			(Widget,BM_Entry *);
	virtual void	entryActivated			(Widget,BM_Entry *);
	virtual void	entryCascading			(Widget,BM_Entry *);
	virtual void	entryEnter				(Widget,BM_Entry *);
	virtual void	entryLeave				(Widget,BM_Entry *);

	// Gets called when the whole thing needs updating
	virtual void	prepareToUpdateRoot		();

	// Gets called to actually update the whole thing
	virtual void	reallyUpdateRoot		();

	// Gets called to update icon appearance
	virtual void	updateAppearance		();

	// Gets called when the personal toolbar folder's name changes
	virtual void	updateToolbarFolderName	();

	// Configure the items
	virtual void	configureXfeCascade		(Widget,BM_Entry *);
	virtual void	configureXfeButton		(Widget,BM_Entry *);
	virtual void	configureXfeBmButton	(Widget,BM_Entry *);
	virtual void	configureXfeBmCascade	(Widget,BM_Entry *);
	virtual void	configureButton			(Widget,BM_Entry *);
	virtual void	configureCascade		(Widget,BM_Entry *);
	virtual void	configureSeparator		(Widget,BM_Entry *);

	// Menu component creation methods
	Widget	createCascadeButton		(Widget menu,BM_Entry * entry,XP_Bool ignore_children);
	Widget	createPushButton		(Widget menu,BM_Entry * entry);
	Widget	createSeparator			(Widget menu);
	Widget	createMoreButton		(Widget menu);

	// Toolbar component creation methods
	Widget	createXfeCascade		(Widget parent,BM_Entry * entry);
	Widget	createXfeButton			(Widget parent,BM_Entry * entry);

	BM_Entry * getRootFolder		();
	BM_Entry * getAddFolder			();
	BM_Entry * getMenuFolder		();

private:

	MWContext *			_bookmarkContext;		// Bookmark context
	XFE_Frame *			_frame;					// The ancestor frame
	XP_Bool				_onlyHeaders;			// Only show headers
	XP_Bool				_fancyItems;			// Fancy items (pixmap & label)
	INTL_CharSetInfo	_charSetInfo;			// Char set info

	char *				_dropAddressBuffer;		// 
	char *				_dropTitleBuffer;		// 
	BM_Date				_dropLastAccess;		// 

	// Item callbacks
	static void		item_armed_cb		(Widget,XtPointer,XtPointer);
	static void		item_disarmed_cb	(Widget,XtPointer,XtPointer);
 	static void		item_activated_cb	(Widget,XtPointer,XtPointer);
 	static void		item_cascading_cb	(Widget,XtPointer,XtPointer);
 	static void		item_enter_cb		(Widget,XtPointer,XtPointer);
 	static void		item_leave_cb		(Widget,XtPointer,XtPointer);
 	static void		item_free_data_cb	(Widget,XtPointer,XtPointer);
	static void		pane_mapping_eh		(Widget,XtPointer,XEvent *,Boolean *);

	// Format item blah blah blah
	static XmString	formatItem			(MWContext *	bookmarkContext, 
										 BM_Entry *		entry, 
										 Boolean		no_indent,
										 int16			charset);

	// Obtain an internationallized XmString from an entry
	static XmString entryToXmString	(MWContext *		bookmarkContext,
									 BM_Entry *			entry,
									 INTL_CharSetInfo	char_set_info);


	void setItemLabelString(Widget menu,BM_Entry * entry);

	void createPixmaps();

	Widget	getLastMoreMenu		(Widget menu);

	// Bookmarks changed notice invoked by MozillaApp
	XFE_CALLBACK_DECL(bookmarksChanged)

    // update the icon appearance
    XFE_CALLBACK_DECL(updateIconAppearance)

    // update the personal toolbar folder
    XFE_CALLBACK_DECL(updatePersonalToolbarFolderName)
};

#endif /* _xfe_bookmark_base_h_ */
