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

//
// Mike Pinkerton, Netscape Communications
//
// Contains:
//
// ¥ CNavCenterContextMenuAttachment
//		A version of the contextual menu attachment that generates the menu from the
//		RDF backend. This class is virtual.
// ¥ CTreeViewContextMenuAttchment
//		Generates the menu assuming the parent is a CHyperTreeFlexTable (tree view)
// ¥ CToolbarButtonContetxMenuAttachment
//		Generates the menu assuming the parent is a toolbar button
// ¥ CToolbarContetxMenuAttachment
//		Generates the menu assuming the parent is a toolbar
//

#pragma once

#include "htrdf.h"
#include "CContextMenuAttachment.h"


class CNavCenterContextMenuAttachment : public CContextMenuAttachment
{
public:	
	CNavCenterContextMenuAttachment(LStream* inStream) : CContextMenuAttachment(inStream) { }
	CNavCenterContextMenuAttachment(ResIDT inMenuID, ResIDT inTextTraitsID) 
		: CContextMenuAttachment(inMenuID, inTextTraitsID) { }
	virtual	~CNavCenterContextMenuAttachment() { };

protected:	

	virtual HT_Cursor NewHTContextMenuCursor ( ) = 0; 
	virtual CommandT ConvertHTCommandToPPCommand ( HT_MenuCmd inCmd ) ;
	virtual LMenu* BuildMenu ( ) ;
	virtual UInt16 PruneMenu ( LMenu* inMenu ) ;

}; // CNavCenterContextMenuAttachment


class CTreeViewContextMenuAttachment : public CNavCenterContextMenuAttachment
{
public:
	enum { class_ID = 'NCxM' } ;		// exact naming due to historical reasons.

	CTreeViewContextMenuAttachment(LStream* inStream) : CNavCenterContextMenuAttachment(inStream) { };
	virtual	~CTreeViewContextMenuAttachment() { };

protected:	
	virtual HT_Cursor NewHTContextMenuCursor ( ) ;

}; // CTreeViewContextMenuAttachment


class CToolbarButtonContextMenuAttachment : public CNavCenterContextMenuAttachment
{
public:
	enum { class_ID = 'TBxM' } ;
	
	CToolbarButtonContextMenuAttachment() : CNavCenterContextMenuAttachment(5000, 0) { }
	virtual	~CToolbarButtonContextMenuAttachment() { };

protected:	
	virtual HT_Cursor NewHTContextMenuCursor ( ) ;

}; // CToolbarButtonContextMenuAttachment


class CToolbarContextMenuAttachment : public CNavCenterContextMenuAttachment
{
public:
	enum { class_ID = 'TlxM' } ;
	
	CToolbarContextMenuAttachment() : CNavCenterContextMenuAttachment(5000, 0) { }
	virtual	~CToolbarContextMenuAttachment() { };

protected:	
	virtual HT_Cursor NewHTContextMenuCursor ( ) ;

}; // CToolbarContextMenuAttachment