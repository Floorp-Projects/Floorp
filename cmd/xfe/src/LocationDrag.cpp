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
   LocationDrag.cpp -- class definitions for browser location drag source
   Created: Alastair Gourlay(SGI) c/o Dora Hsu<dora@netscape.com>, 26 Nov 1996
 */



// classes:
//      XFE_LocationDrag
//

#include <stdio.h>
#include <Xm/Xm.h>
#include <Xm/AtomMgr.h>
#include <Xfe/ButtonP.h>
#include "icondata.h"
#include "LocationDrag.h"

#ifdef DEBUG_sgidev
#define XDEBUG(x) x
#else
#define XDEBUG(x)
#endif

// constructor

XFE_LocationDrag::XFE_LocationDrag(Widget w) : XFE_DragNetscape(w)
{
    _dragDataURL=NULL;
}

// destructor

XFE_LocationDrag::~XFE_LocationDrag()
{
    if (_dragDataURL) {
        XP_FREE(_dragDataURL);
        _dragDataURL=NULL;
    }
}


// set drag data, for later use by drag callback

void XFE_LocationDrag::setDragData(URL_Struct* u)
{
    if (_dragDataURL)
        XP_FREE(_dragDataURL);
    
    _dragDataURL=((u && u->address) ? XP_STRDUP(u->address) : 0);
}


// decide if this drag is interesting

int XFE_LocationDrag::dragStart(int,int)
{
    if (!_dragDataURL)
        return FALSE;

    if (isFileURL(_dragDataURL))
        dragFilesAsLinks(TRUE);

    setDragIcon(&LocationProxy);

    return TRUE;
}

void XFE_LocationDrag::dragComplete()
{
	/*
	 * The proxy icon button never received Disarm() and Enter() events 
	 * cause the drag and drop event handlers are stealing them.  The 
	 * "correct" way to fix this would be to have the XfeButton button 
	 * install dnd translations/actions and have the XFE_DragBase class
	 * deal with this...but that would involve major work.
	 * would also mean that the drag and drop code would have to deal with
	 * 
	 * The most reasonable course of action is deal with this unique case
	 * here by forcing the proxy icon to return to the "normal" state 
	 * everytime the drag operation is complete.
	 */
	if (XfeIsAlive(_widget) && XfeIsButton(_widget))
	{
		_XfeButtonDisarm(_widget,NULL,NULL,NULL);
		_XfeButtonLeave(_widget,NULL,NULL,NULL);
	}
}

// specify types for this particular drag

void XFE_LocationDrag::targets()
{
    _numTargets=2;
    _targets=new Atom[_numTargets];

    _targets[0]=_XA_NETSCAPE_URL;
    _targets[1]=XA_STRING;

    setFileTarget(_XA_NETSCAPE_URL);
}

// specify operations for this particular drag

void XFE_LocationDrag::operations()
{
    _operations=XmDROP_COPY;
}

// provide data for requested target from targets() list

char *XFE_LocationDrag::getTargetData(Atom target)
{
    // WARNING - data *must* be allocated with Xt malloc API, or Xt
    // will spring a leak!

    if (!_dragDataURL)
        return NULL;
    
    if (target==_XA_NETSCAPE_URL) {
        // translate drag data to NetscapeURL format
        XFE_URLDesktopType urlData;

        urlData.createItemList(1);
        urlData.url(0,_dragDataURL);
        return (char*) XtNewString(urlData.getString());
    }

    if (target==XA_STRING) {
        // return the URL
        return (char*) XtNewString(_dragDataURL);        
    }

    return NULL;
}

