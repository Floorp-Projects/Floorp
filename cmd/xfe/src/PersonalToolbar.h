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
/* Name:		PersonalToolbar.h										*/
/* Description:	XFE_PersonalToolbar component header file.				*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/



#ifndef _xfe_persoanl_toolbar_h_
#define _xfe_persoanl_toolbar_h_

#include "Component.h"
#include "BookmarkBase.h"
#include "ToolboxItem.h"

class XFE_PersonalDrop;
class XFE_Logo;
class XFE_PopupMenu;

class XFE_PersonalToolbar : public XFE_ToolboxItem,
							public XFE_BookmarkBase
{
public:  
	
	XFE_PersonalToolbar		(MWContext *	bookmarkContext,
							 XFE_Toolbox *	parent_toolbox,
							 const char *	name,
							 XFE_Frame *	frame);

	virtual ~XFE_PersonalToolbar();

	// Access methods
	Widget			getToolBarWidget	();
	Widget			getLastItem			();
	Widget			getIndicatorItem	();

	void			configureIndicatorItem		(BM_Entry * entry);

	void			setRaised			(XP_Bool);

	void			addEntry				(const char *	address,
											 const char *	title,
											 BM_Date		lastAccess);
	// DND feedback methods
	Widget			getDropTargetItem		();
	unsigned char	getDropTargetLocation	();

	void 			setDropTargetItem		(Widget	item,int x);
	void 			clearDropTargetItem		();

	static void			setToolbarFolder		(BM_Entry *		entry,
												 XP_Bool		notify);

	static BM_Entry *	getToolbarFolder		();

	static char *		getToolbarFolderName	();

	static XP_Bool		hasToolbarFolder		();

protected:

	// Override AbstractToolbar methods
	virtual void	update() {}

	// Override BookmarkBase methods
	virtual void	configureXfeButton		(Widget,BM_Entry *);
	virtual void	configureXfeCascade		(Widget,BM_Entry *);
	virtual void	prepareToUpdateRoot		();
	virtual void	reallyUpdateRoot		();
	virtual void	updateAppearance		();
	virtual void	updateToolbarFolderName	();

private:

	Widget				m_toolBar;			// The toolbar 
	XFE_PersonalDrop *  m_dropSite;			// The proxy icon drop site
	BM_Entry *			m_toolBarFolder;	// The toolbar folder

	// DND feedback members
	Widget				m_dropTargetItem;		// The drop target item
	unsigned char		m_dropTargetLocation;	// The drop target location
	int					m_dropTargetPosition;	// The drop target position

	void			destroyToolbarWidgets			();

	void			addDefaultToolbarFolder			();

	void			addDefaultPersonalEntries		(char *		configString,
													 BM_Entry *	header);

	XP_Bool			isToolbarFolderValid			();

	//
	// Popup menu stuff
	//
	XFE_PopupMenu *			m_popup;			// The popup menu
	static MenuSpec			m_popupMenuSpec[];	// The popup menu spec

	static void				popupCB	(Widget,XtPointer,XtPointer);
	
	void handlePopup		(Widget,XEvent *);
};

#endif /* _xfe_persoanl_toolbar_h_ */
