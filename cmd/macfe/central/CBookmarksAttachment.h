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

//	Handle creating and maintaining the top-level bookmarks menu. It pulls the info
//	out of the RDF container the user designates as their "quickfile menu" and listens
//	to the messages from RDF to update it.

#pragma once

#ifndef CBOOKMARKSATTACHMENT_H_
#define CBOOKMARKSATTACHMENT_H_

#include "CRDFNotificationHandler.h"

#include "PascalString.h"
#include <LAttachment.h>
#include <LMenu.h>
#include <LArray.h>

/***********************************************************************************
 * CBookmarksAttachment
 * Processes bookmark menu commands -- should be attached to application
 ***********************************************************************************/
class CBookmarksAttachment: public LAttachment, public CRDFNotificationHandler
{
public:
	// ¥¥ constructors
						CBookmarksAttachment();
	// ¥¥Êevents
	virtual void		ExecuteSelf( MessageT inMessage, void* ioParam );

//	static void			AddToBookmarks( BM_Entry* newBookmark );
	static void			AddToBookmarks( const char* url, const CStr255& title );

	static LMenu*		GetMenu();
	static void			InvalidateMenu() { sInvalidMenu = true; }
	static void			UpdateMenu();

	static void			RemoveMenus();
	static void			InstallMenus();
	
	void				InitQuickfileView ( ) ;

protected:
	static void			FillMenuFromList( HT_Resource top, LMenu* newMenu, int& nextMenuID, int whichItem );

	virtual	void		HandleNotification( HT_Notification	notifyStruct, HT_Resource node, HT_Event event);

	static LMenu*		sMenu;
	static Boolean		sInvalidMenu;
	static LArray		sMenusList;
	static HT_View		sQuickfileView;		// called quickfile because of HT API

};

#endif
