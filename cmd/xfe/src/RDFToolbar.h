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
//----------------------------------------------------------------------
//
// Name:		RDFToolbar.h
// Description:	XFE_RDFToolbar class header.
//				Make a toolbar from an RDF node
// Author:		Stephen Lamm <slamm@netscape.com>
// Date:		Tue Jul 28 11:28:50 PDT 1998
//
//----------------------------------------------------------------------

#ifndef _xfe_rdftoolbar_h_
#define _xfe_rdftoolbar_h_

#include "RDFMenuToolbarBase.h"
#include "ToolboxItem.h"
#include "htrdf.h"

#include <X11/Intrinsic.h>

class XFE_Frame;
class XFE_Logo;
class XFE_Toolbox;
class XFE_RDFToolbarDrop;

class XFE_RDFToolbar : public XFE_ToolboxItem,
                       public XFE_RDFMenuToolbarBase
{
public:
    
 	XFE_RDFToolbar(XFE_Frame * frame, XFE_Toolbox * toolbox,
                   HT_View toolbar_root);

	virtual ~XFE_RDFToolbar ();

	// Access methods
	Widget			getFirstItem		();
	Widget			getLastItem			();
	Widget			getIndicatorItem	();
	void			configureIndicatorItem		(HT_Resource entry);

#ifdef NOT_YET
	// update all the commands in the toolbar.
	XFE_CALLBACK_DECL(update)
    // update the toolbar appearance
    XFE_CALLBACK_DECL(updateToolbarAppearance)
#endif
	// Method version of the callback above. 
    // Used by the frame to force the toolbar to update itself
	virtual void	update();

    // Override RDFMenuToolbarBase methods
    virtual void notify(HT_Resource n, HT_Event whatHappened);

	//////////////////////////////////////////////////////////////////////
	//                                                                  //
	// XFE_RDFToolbar notifications                                     //
	//                                                                  //
	//////////////////////////////////////////////////////////////////////
	static const char *	navigateToUrlNotice;

    // DND feedback methods
    Widget          getDropTargetItem       ();
    unsigned char   getDropTargetLocation   ();

    void            setDropTargetItem       (Widget item,int x);
    void            clearDropTargetItem     ();

protected:
	void	   setRaised                (XP_Bool);
    void       destroyToolbarWidgets    ();
    XP_Bool    isToolbarFolderValid     ();

    void       addItem                  (HT_Resource);

    // Override RDFMenuToolbarBase methods
	virtual void	configureXfeCascade		(Widget, HT_Resource);

    // Toolbar component creation methods
    void	createCascade		(Widget parent, HT_Resource entry);

    // Override RDFMenuToolbarBase methods
	virtual void	prepareToUpdateRoot		();
	virtual void	updateRoot      		();

    // Gets called to update icon appearance
    virtual void    updateAppearance        ();

private:

    XFE_Frame *	             _frame;
    Widget                   _toolbar;
    XFE_RDFToolbarDrop *     _toolbarDropSite;

    // DND feedback members
    Widget             _dropTargetItem;       // The drop target item
    unsigned char      _dropTargetLocation;   // The drop target location
    int                _dropTargetPosition;   // The drop target position

  // callbacks
    static void tooltipCB(Widget, XtPointer,  XmString *, Boolean *);
    static void docStringSetCB(Widget, XtPointer, XmString *, Boolean *);
    static void docStringCB(Widget, XtPointer, unsigned char, XmString);

	// update a specific command in the toolbar.
	XFE_CALLBACK_DECL(updateCommand)

	// update the icon appearance
    XFE_CALLBACK_DECL(updateIconAppearance)

	// Url bar activated
	XFE_CALLBACK_DECL(urlBarTextActivatedNotice)
};

#endif /*_xfe_rdftoolbar_*/
