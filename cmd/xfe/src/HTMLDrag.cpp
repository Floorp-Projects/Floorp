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
   HTMLDrag.cpp -- class definition for the HTMLView drag class
   Created: Alastair Gourlay <sgidev@netscape.com>, 1-Jan-96.
 */



#include <stdio.h>
#include <Xm/Xm.h>
#include <Xm/AtomMgr.h>
#include "HTMLDrag.h"
#include "layers.h"

#if defined(DEBUG_sgidev) || defined(DEBUG_tao)
#define XDEBUG(x) x
#else
#define XDEBUG(x)
#endif

//
// XFE_HTMLDrag class
//

static void HTMLDragDestroyCb(Widget,XtPointer,XtPointer);

// provide access to HTML Drag class from old XFE C code.

extern "C" void XFE_HTMLDragCreate(Widget widget,MWContext *context)
{
    if (widget && context) {
        XFE_HTMLDrag *htmlDrag=new XFE_HTMLDrag(widget,context);
        XtAddCallback(widget,XmNdestroyCallback,HTMLDragDestroyCb,(XtPointer)htmlDrag);
    }
}

// auto-cleanup of classes created with C API
static void HTMLDragDestroyCb(Widget widget,XtPointer cd, XtPointer)
{
    XDEBUG(printf("HTMLDragDestroyCb(%x)\n",widget));
    if (cd) {
        XFE_HTMLDrag *htmlDrag=(XFE_HTMLDrag*)cd;
        delete htmlDrag;
    }
}

// constructor

XFE_HTMLDrag::XFE_HTMLDrag(Widget w,MWContext *context) : XFE_DragNetscape(w)
{
    _context=context;
}

// destructor

XFE_HTMLDrag::~XFE_HTMLDrag()
{
}

// Called  by xfe.c, in the Layer event handler when a link
// is armed by a Button1Down.
// (Motif DND needs a hook into the Layer event mgmt in order
// to process drags on links within layers.)

static CL_Layer *_dragLayer=NULL;

extern "C" void fe_HTMLDragSetLayer(CL_Layer *layer)
{
    // record layer of Button1 press on a link in a layer
    _dragLayer=layer;
}


// Extract URL from context, layer, x, y
// (caller must free the returned URL_Struct)
// Note: XFE_HTMLView has similar code, but FE_HTMLDrag doesn't
// have an XFE_HTMLView*, only an MWContext*.

URL_Struct *
XFE_HTMLDrag::urlAtPosition(int x, int y, CL_Layer *layer)
{
    URL_Struct *urlStruct = NULL;

    if (!_context)
        return NULL;

    // adjust for document origin
    int docX = x + CONTEXT_DATA(_context)->document_x;
    int docY = y + CONTEXT_DATA(_context)->document_y;

    // adjust layer origin
    if (layer) {
        docX -= CL_GetLayerXOrigin(layer);
        docY -= CL_GetLayerYOrigin(layer);
    }    

    LO_Element *le=LO_XYToElement(_context,docX,docY,layer);

    if (!le)
        return NULL;

    switch (le->type) {
    case LO_TEXT:
        if (le->lo_text.anchor_href && le->lo_text.anchor_href->anchor)
            urlStruct = NET_CreateURLStruct ((char *)le->lo_text.anchor_href->anchor,
                                             NET_DONT_RELOAD);
        break;
    case LO_IMAGE:
        {
        long ix = le->lo_image.x + le->lo_image.x_offset;
        long iy = le->lo_image.y + le->lo_image.y_offset;
        long mx = docX - ix - le->lo_image.border_width;
        long my = docY - iy - le->lo_image.border_width;                    
        // check for client-side image map
        if (le->lo_image.image_attr->usemap_name != NULL) {
            LO_AnchorData *anchorData=LO_MapXYToAreaAnchor(_context,
                                                           (LO_ImageStruct *)le,
                                                           mx, my);
            if (anchorData && anchorData->anchor)
                urlStruct=NET_CreateURLStruct ((char *)anchorData->anchor,
                                               NET_DONT_RELOAD);
        }
        // check for regular image or server-side image map
        else if (le->lo_image.anchor_href && le->lo_image.anchor_href->anchor) {
            urlStruct=NET_CreateURLStruct ((char *)le->lo_image.anchor_href->anchor,
                                           NET_DONT_RELOAD);
            // if this is an image map (and not an about: link), add the
            //coordinates to the URL
            if (XP_STRNCASECMP(urlStruct->address, "about:",6)!=0 &&
                le->lo_image.image_attr->attrmask & LO_ATTR_ISMAP) {
                NET_AddCoordinatesToURLStruct (urlStruct,
                                               ((mx < 0) ? 0 : mx),
                                               ((my < 0) ? 0 : my));
            }
        }
        }
        break;
    default:
        urlStruct=NULL;
        break;
    }

    return urlStruct;
}

//
//  drag virtual methods - derived class can override
//

// decide if this drag is interesting

int XFE_HTMLDrag::dragStart(int x,int y)
{
    XDEBUG(printf("XFE_HTMLDrag::dragStart(%d,%d)\n",x,y));

    _dragURLStruct=urlAtPosition(x,y,_dragLayer);

    // clean up drag layer for next click
    _dragLayer=NULL;
    
    if (!_dragURLStruct || !_dragURLStruct->address || strlen(_dragURLStruct->address)==0)
        return FALSE;

    // reject URL's which don't point to document:
    //  addbook:add URL's
    //  mailbox:displayattachments URL
    //  javascript code
    //  about: urls
    //  mailto: urls
    if (XP_STRNCASECMP(_dragURLStruct->address,"addbook:add",11)==0 ||
        XP_STRNCASECMP(_dragURLStruct->address,"about:",6)==0 ||
        XP_STRNCASECMP(_dragURLStruct->address,"mailto:",7)==0 ||
        XP_STRNCASECMP(_dragURLStruct->address,"mailbox:displayattachments",26)==0 ||
        XP_STRNCASECMP(_dragURLStruct->address,"javascript:",11)==0)
        return FALSE;

    // disarm link and clear selection, since XmDragStart() will steal the Btn1Up
    XtCallActionProc(_widget,"DisarmLink",(XEvent*)&_dragButtonEvent,NULL,0);
    LO_ClearSelection(_context);
    // files get dragged to desktop as a symbolic link
    if (isFileURL(_dragURLStruct->address))
        dragFilesAsLinks(TRUE);
    // try to be sensitive to type of URL
#ifdef MOZ_MAIL_NEWS
    setDragIconForType(guessUrlMimeType(_dragURLStruct->address));
#endif

    return TRUE;
}

// specify types for this particular drag

void XFE_HTMLDrag::targets()
{
    _numTargets=2;
    _targets=new Atom[_numTargets];

    _targets[0]=_XA_NETSCAPE_URL;
    _targets[1]=XA_STRING;

    setFileTarget(_XA_NETSCAPE_URL);
}

// specify operations for this particualr drag
void XFE_HTMLDrag::operations()
{
    _operations=XmDROP_COPY;
}

// provide data for requested target from targets() list
char *XFE_HTMLDrag::getTargetData(Atom target)
{
    // WARNING - data *must* be allocated with Xt malloc API, since Xt
    // will free the data with XtFree()
    
    if (target==_XA_NETSCAPE_URL) {
        // translate drag data to NetscapeURL format
        XFE_URLDesktopType urlData;

        urlData.createItemList(1);
        urlData.url(0,_dragURLStruct->address);
        return (char*)XtNewString(urlData.getString());
    }

    if (target==XA_STRING) {
        // return the URL
        return (char*)XtNewString(_dragURLStruct->address);
    }

    return NULL;
}

// clean up after drag
void XFE_HTMLDrag::dragComplete()
{
    if (_dragURLStruct) {
        NET_FreeURLStruct(_dragURLStruct);
        _dragURLStruct=NULL;
    }
}

