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
   DragDrop.cpp -- Drag and Drop classes to hide Motif's API, and work with Desktop files.
   Created: Alastair Gourlay(SGI) c/o Dora Hsu<dora@netscape.com>, 26 Nov 1996
 */



// Classes in this file:
//      XFE_DragBase
//        XFE_DragDesktop
//          XFE_DragNetscape
//      XFE_DropBase
//        XFE_DropDesktop
//          XFE_DropNetscape
//

#include <stdlib.h>
#include <net.h>
#include <netdb.h>
#include <icondata.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <xp_mem.h>
#include <xp_str.h>
#include <xpassert.h>
#include <xfe.h>
#include "DragDrop.h"
#include <Xm/DisplayP.h>

#if defined(SOLARIS)||defined(AIX)||defined(UNIXWARE)
extern "C" int gethostname(char *, int);
#endif

#if defined(SCO_SV)
#include <sys/socket.h>  // Need MAXHOSTNAMELEN
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

#if defined(DEBUG_sgidev) 
#define XDEBUG(x) x
#else
#define XDEBUG(x)
#endif

#ifdef MOZ_MAIL_NEWS
// from libmsg/msgglue.c
extern "C" XP_Bool MSG_RequiresMailMsgWindow(const char*);
extern "C" XP_Bool MSG_RequiresNewsMsgWindow(const char*);
extern "C" XP_Bool MSG_RequiresBrowserWindow(const char*);
#endif

//
// this list defines platforms for which Motif drag and drop is usable.
//

// test - enable Drag and Drop for all platforms - need to find out now
// if there are unavoidable Motif or X11 bugs on any platform.

//#if defined(IRIX) || defined(SOLARIS) || defined(HPUX) || defined(UNIXWARE)
#define DRAG_ENABLED
//#endif
//#if defined(IRIX) || defined(SOLARIS) || defined(HPUX) || defined(UNIXWARE)
#define DROP_ENABLED
//#endif

// default FTR type
static const char DEFAULT_FTR_TYPE[]="Unknown";

// global drag information
Widget XFE_DragBase::_activeDragShell=NULL;

// Atoms, initialized by InitializeDisplayInfo(), called from drag and drop
// base class constructors

Atom XFE_DragBase::_XA_TARGETS=None;
Atom XFE_DragBase::_XA_DELETE=None;
Atom XFE_DragBase::_XA_MULTIPLE=None;
Atom XFE_DragBase::_XA_TIMESTAMP=None;
Atom XFE_DragBase::_XA_NULL=None;
Atom XFE_DragBase::_XA_SGI_ICON=None;
Atom XFE_DragBase::_XA_SGI_ICON_TYPE=None;
Atom XFE_DragBase::_XA_SGI_FILE=None;
Atom XFE_DragBase::_XA_FILE_NAME=None;
Atom XFE_DragBase::_XA_NETSCAPE_URL=None;

Atom XFE_DropBase::_XA_TARGETS=None;
Atom XFE_DropBase::_XA_DELETE=None;
Atom XFE_DropBase::_XA_NULL=None;
Atom XFE_DropBase::_XA_SGI_ICON=None;
Atom XFE_DropBase::_XA_SGI_FILE=None;
Atom XFE_DropBase::_XA_SGI_URL=None;
Atom XFE_DropBase::_XA_FILE_NAME=None;
Atom XFE_DropBase::_XA_NETSCAPE_URL=None;

static char *localHostName=NULL;

static void InitializeDisplayInfo(Widget widget)
{
    static int idi_firstTime=TRUE;
    if (!idi_firstTime)
        return;
    idi_firstTime=FALSE;

    // atoms used for drag and drop
    XFE_DragBase::_XA_TARGETS=XmInternAtom(XtDisplay(widget),"TARGETS",FALSE);
    XFE_DragBase::_XA_DELETE=XmInternAtom(XtDisplay(widget),"DELETE",FALSE);
    XFE_DragBase::_XA_MULTIPLE=XmInternAtom(XtDisplay(widget),"MULTIPLE",FALSE);
    XFE_DragBase::_XA_TIMESTAMP=XmInternAtom(XtDisplay(widget),"TIMESTAMP",FALSE);
    XFE_DragBase::_XA_NULL=XmInternAtom(XtDisplay(widget),"NULL",FALSE);
    XFE_DragBase::_XA_SGI_ICON=XmInternAtom(XtDisplay(widget),"_SGI_ICON",FALSE);
    XFE_DragBase::_XA_SGI_ICON_TYPE=XmInternAtom(XtDisplay(widget),"_SGI_ICON_TYPE",FALSE);
    XFE_DragBase::_XA_SGI_FILE=XmInternAtom(XtDisplay(widget),"SGI_FILE",FALSE);
    XFE_DragBase::_XA_FILE_NAME=XmInternAtom(XtDisplay(widget),"FILE_NAME",FALSE);
    XFE_DragBase::_XA_NETSCAPE_URL=XmInternAtom(XtDisplay(widget),"_NETSCAPE_URL",FALSE);

    XFE_DropBase::_XA_TARGETS=XmInternAtom(XtDisplay(widget),"TARGETS",FALSE);
    XFE_DropBase::_XA_DELETE=XmInternAtom(XtDisplay(widget),"DELETE",FALSE);
    XFE_DropBase::_XA_NULL=XmInternAtom(XtDisplay(widget),"NULL",FALSE);
    XFE_DropBase::_XA_SGI_ICON=XmInternAtom(XtDisplay(widget),"_SGI_ICON",FALSE);
    XFE_DropBase::_XA_SGI_FILE=XmInternAtom(XtDisplay(widget),"SGI_FILE",FALSE);
    XFE_DropBase::_XA_SGI_URL=XmInternAtom(XtDisplay(widget),"_SGI_URL",FALSE);
    XFE_DropBase::_XA_FILE_NAME=XmInternAtom(XtDisplay(widget),"FILE_NAME",FALSE);
    XFE_DropBase::_XA_NETSCAPE_URL=XmInternAtom(XtDisplay(widget),"_NETSCAPE_URL",FALSE);

    // local hostname, used to process _SGI_ICON drag
    char h[MAXHOSTNAMELEN+1];
    if (gethostname(h, MAXHOSTNAMELEN)==0) {
        h[MAXHOSTNAMELEN]='\0';
        localHostName=strdup(h);
    }
    else {
        localHostName="localhost";
    }
}


//
// XFE_DragBase
//

// globally useful utilities

char *XFE_DragBase::guessUrlMimeType(const char *data)
{    
    // try to determine type of attachment

    // regular file
    if (XP_STRNCASECMP(data,"file:",5)==0 || NET_URL_Type(data)==0) {
        const char *fileData=data;
        if (XP_STRNCASECMP(data,"file:",5)==0) // skip file: prefix
            fileData=data+5;
        if (fe_isDir((char*)fileData))
            return "_directory"; // interal type, since there's no mime type
        NET_cinfo *pMimeInfo = NET_cinfo_find_type((char*)data);
        if ((pMimeInfo) && (pMimeInfo->type))
            return pMimeInfo->type;
        else
            return APPLICATION_OCTET_STREAM;
    }

    // addressbook entry
    else if (XP_STRNCASECMP(data,"addbook:",8)==0)
        return "text/x-vcard";

#if defined(MOZ_MAIL_NEWS)
    // mail or news message
    else if (MSG_RequiresMailMsgWindow(data) || MSG_RequiresNewsMsgWindow(data))
        return "message/rfc822";

    // document URL - use internal id, since there's no URL mime type
    else if (MSG_RequiresBrowserWindow(data))
        return "_url";
#endif  /* MOZ_MAIL_NEWS */

    // fallback to generic type
    else
        return UNKNOWN_CONTENT_TYPE;
}


// callback wrappers
void XFE_DragBase::ButtonPressCb(Widget w,XtPointer cd,XEvent *event,Boolean*)
{
    XFE_DragBase *d=(XFE_DragBase*)cd;
    if (d &&
        event->xany.type==ButtonPress &&
        event->xbutton.button==1) {
        d->_dragWidget=w;
        d->buttonPressCb((XButtonPressedEvent*)event);
    }
}

void XFE_DragBase::ButtonMotionCb(Widget w,XtPointer cd,XEvent *event,Boolean *eatEvent)
{
    XFE_DragBase *d=(XFE_DragBase*)cd;
    if (d &&
        w==d->_dragWidget &&
        event->xany.type==MotionNotify) {
        *eatEvent=d->buttonMotionCb((XMotionEvent*)event);
    }    
}

Boolean XFE_DragBase::DragConvertCb(Widget w,Atom*,Atom *target,Atom *type,
                                    XtPointer *value,unsigned long *length,int *format)
{
    if (!w || !XmIsDragContext(w))
        return FALSE;
        
    XtPointer cd;
    XtVaGetValues(w,XmNclientData,&cd,NULL);
    XFE_DragBase *d=(XFE_DragBase*)cd;

    if (d) {
        return d->dragConvertCb(target,type,value,length,format);
    }
    return FALSE;
}

void XFE_DragBase::OperationChangedCb(Widget w,XtPointer cd,XtPointer)
{
    if (!w || !XmIsDragContext(w))
        return;
        
    XFE_DragBase *d=(XFE_DragBase*)cd;

    // nyi - what callback struct info should be passed to method?
    if (d) {
        d->operationChangedCb();
    }
}

void XFE_DragBase::DragMotionCb(Widget w,XtPointer cd,XtPointer cb)
{
    if (!w || !XmIsDragContext(w))
        return;
        
    XFE_DragBase *d=(XFE_DragBase*)cd;
    XmDragMotionCallbackStruct *dmcb=(XmDragMotionCallbackStruct*)cb;

    // nyi - what callback struct info should be passed to method?
    if (d) {
        // record (x,y) of pointer during drag
        if (dmcb) {
            d->_dragEventX=dmcb->x;
            d->_dragEventY=dmcb->y;
        }        
        d->dragMotionCb();
    }
}

void XFE_DragBase::DropStartCb(Widget w,XtPointer cd,XtPointer)
{
    if (!w || !XmIsDragContext(w))
        return;
        
    XFE_DragBase *d=(XFE_DragBase*)cd;

    // nyi - what callback struct info should be passed to method?
    if (d) {
        d->dropStartCb();
    }
}

void XFE_DragBase::DropSiteEnterCb(Widget w,XtPointer cd,XtPointer cb)
{
    if (!w || !XmIsDragContext(w))
        return;
        
    XFE_DragBase *d=(XFE_DragBase*)cd;

    XmDropSiteEnterCallbackStruct *dcb=(XmDropSiteEnterCallbackStruct*)cb;
    
    // nyi - what callback struct info should be passed to method?
    if (d) {
        d->dropSiteEnterCb(dcb->operations);
    }
    XDEBUG(printf("  operation=%x, operations=%x,status=%d\n",
                  dcb->operation,dcb->operations,dcb->dropSiteStatus));
}

void XFE_DragBase::DropSiteLeaveCb(Widget w,XtPointer cd,XtPointer)
{
    if (!w || !XmIsDragContext(w))
        return;
        
    XFE_DragBase *d=(XFE_DragBase*)cd;

    // nyi - what callback struct info should be passed to method?
    if (d) {
        d->dropSiteLeaveCb();
    }
}

void XFE_DragBase::DropFinishCb(Widget w,XtPointer cd,XtPointer)
{
    if (!w || !XmIsDragContext(w))
        return;
        
    XFE_DragBase *d=(XFE_DragBase*)cd;

    // nyi - what callback struct info should be passed to method?
    if (d) {
        d->dropFinishCb();
    }
}

void XFE_DragBase::DragDropFinishCb(Widget w,XtPointer cd,XtPointer)
{
    if (!w || !XmIsDragContext(w))
        return;
        
    XFE_DragBase *d=(XFE_DragBase*)cd;

    // nyi - what callback struct info should be passed to method?
    if (d) {
        d->dragDropFinishCb();
    }
}

void XFE_DragBase::DestroyCb(Widget w,XtPointer,XtPointer)
{
    if (!w || !XmIsDragContext(w))
        return;
        
    //XFE_DragBase *d=(XFE_DragBase*)cd;

    XDEBUG(printf("XFE_DragBase::DestroyCb()\n"));
}

// constructor

// The drag classes will monitor drags from the widget passed
// to the constructor. Simple drag classes can just pass
// one widget to the constructor. Multi-item objects can use
// one XFE_Drag class with additional widgets, specified by
// addDragWidget(). During the drag, the field _dragWidget
// will point to the widget currently being dragged.

XFE_DragBase::XFE_DragBase(Widget widget)
{
    _widget=widget;
    _dragWidget=NULL;
    _dragThreshold=4;
    _dragStarted=FALSE;
    _buttonPressed=FALSE;
    _dragStartX=0;
    _dragStartY=0;
    _dragEventX=0;
    _dragEventY=0;
    _dragContext=NULL;
    _dragIcon=NULL;    
    _targets=NULL;
    _numTargets=0;
    _dragFilesAsLinks=FALSE;

    // default icon is blank document
    _dragIconData=&GUnknown;
    _dragIconPixmap=None;
    _dragIconPixmapMask=None;
    _dragIconWidth=0;
    _dragIconHeight=0;
    _dragHotX=0;
    _dragHotY=0;

    InitializeDisplayInfo(widget);

    addDragWidget(widget);

#ifdef IRIX
    // hack for SGI Motif - force it to sgidladd/dlopen conversion library
    // now, not on the first drag - it can be slow, and blows the usability
    // out the water - an early button release leaves a permanent cursor
    // window turd. Note: performance hit is present only in 6.2 and
    // 6.3. 6.3-R10K and 6.4 were improved greatly.
    static int firstTime=TRUE;
    if (firstTime) {
        XDEBUG(printf("Hacking XmDragContext initialization {\n"));
        firstTime=FALSE;
        Atom tl[1]={ XA_STRING };
        Arg args[2];
        int n=0;
        XtSetArg(args[n],XmNexportTargets,&tl);n++;
        XtSetArg(args[n],XmNnumExportTargets,1);n++;
        XtDestroyWidget(XtCreateWidget("dragContext",xmDragContextClass,
                                       XmGetXmDisplay(XtDisplay(widget)),
                                       args,n));
        XDEBUG(printf("}\n"));
    }
#endif
}


// destructor

XFE_DragBase::~XFE_DragBase()
{
    XDEBUG(printf("XFE_DragBase::~XFE_DragBase()\n"));
    
    // destroy drag icon widget
    // usually destroyed in dragDropFinishCb, but catch case of mid-drag Destroy.
    if (_dragIcon) {
        XtDestroyWidget(_dragIcon);
        _dragIcon=NULL;
    }

    // clean up drag icon data
    setDragIcon(NULL);
}


//
// drag methods
//

void XFE_DragBase::addDragWidget(Widget widget)
{    
    if (widget) {
        // catch button 1 press and motion
        XtInsertEventHandler(widget,ButtonPressMask,FALSE,
                             ButtonPressCb,(XtPointer)this,XtListHead);
        XtInsertEventHandler(widget,Button1MotionMask,FALSE,
                             ButtonMotionCb,(XtPointer)this,XtListHead);

        // clean up when widget is destroyed
        XtAddCallback(widget,XmNdestroyCallback,dragWidgetDestroyCb,(XtPointer)this);
    }
}

void XFE_DragBase::dragWidgetDestroyCb(Widget widget,XtPointer cd,XtPointer)
{
    if (widget && cd) {
        XDEBUG(printf("XFE_DragBase::dragWidgetDestroyCb(%s)\n",XtName(widget)));
        XtRemoveEventHandler(widget,ButtonPressMask,FALSE,
                             ButtonPressCb,cd);
        XtRemoveEventHandler(widget,Button1MotionMask,FALSE,
                             ButtonMotionCb,cd);
        // _dragIcon widget is a child of the _dragWidget.
        // It will get destroyed by Xt, so clean up our pointer.
        XFE_DragBase *db=(XFE_DragBase*)cd;
        db->_dragIcon=NULL;
    }
}

void XFE_DragBase::dragInitialize()
{
    if (_dragContext!=NULL) {
        XDEBUG(printf("DRAG IN PROGRESS - new drag not started\n"));
        return;
    }

    if (!_dragWidget)
        return;
    
    _dragFilesAsLinks=FALSE;
    
    // let derived class decide how to handle this drag
    if (dragStart(_dragStartX,_dragStartY)==FALSE) {
        dragDropFinishCb();
        return;
    }
    
    targets();
    operations();

    if (_numTargets==0 || _operations==0) {
        dragDropFinishCb();
        return;
    }

    // enable desktop LINK behavior, if application class asks
    if (_dragFilesAsLinks) {
        _operations|=(unsigned int)XmDROP_LINK;
    }
    
    _dragIcon=NULL;
    // create drag icons if necessary
    // (i.e. first time, and after setDragIcon() )
#ifdef DRAG_ENABLED
    if (_dragIconData) {
        if (_dragIconPixmap==None)
            _dragIconPixmap=XCreateBitmapFromData(XtDisplay(_dragWidget),
                                                  XtWindow(_dragWidget),
                                                  (char*)_dragIconData->mono_bits,
                                                  _dragIconData->width,
                                                  _dragIconData->height);
        if (_dragIconPixmapMask==None)
            _dragIconPixmapMask=XCreateBitmapFromData(XtDisplay(_dragWidget),
                                                      XtWindow(_dragWidget),
                                                      (char*)_dragIconData->mask_bits,
                                                      _dragIconData->width,
                                                      _dragIconData->height);
        Arg args[20];
        int n=0;
        XtSetArg(args[n],XmNwidth,_dragIconData->width);n++;
        XtSetArg(args[n],XmNheight,_dragIconData->height);n++;
        XtSetArg(args[n],XmNpixmap,_dragIconPixmap);n++;
        XtSetArg(args[n],XmNhotX,_dragHotX);n++;
        XtSetArg(args[n],XmNhotY,_dragHotY);n++;
        if (_dragIconPixmapMask) {
            XtSetArg(args[n],XmNmask,_dragIconPixmapMask);n++;
        }
        _dragIcon=XmCreateDragIcon(_dragWidget,"dragIcon",args,n);
#endif
    }

    // callback records
    static XtCallbackRec operationChangedCbRec[]= {
        {OperationChangedCb,NULL},{NULL,NULL}
    };
    static XtCallbackRec dragMotionCbRec[]= {
        {DragMotionCb,NULL},{NULL,NULL}
    };
    static XtCallbackRec dropStartCbRec[]= {
        {DropStartCb,NULL},{NULL,NULL}
    };
    static XtCallbackRec dropSiteEnterCbRec[]= {
        {DropSiteEnterCb,NULL},{NULL,NULL}
    };
    static XtCallbackRec dropSiteLeaveCbRec[]= {
        {DropSiteLeaveCb,NULL},{NULL,NULL}
    };
    static XtCallbackRec dropFinishCbRec[]= {
        {DropFinishCb,NULL},{NULL,NULL}
    };
    static XtCallbackRec dragDropFinishCbRec[]= {
        {DragDropFinishCb,NULL},{NULL,NULL}
    };
    static XtCallbackRec destroyCbRec[]= {
        {DestroyCb,NULL},{NULL,NULL}
    };
    operationChangedCbRec[0].closure=this;
    dragMotionCbRec[0].closure=this;
    dropStartCbRec[0].closure=this;
    dropSiteEnterCbRec[0].closure=this;
    dropSiteLeaveCbRec[0].closure=this;
    dropFinishCbRec[0].closure=this;
    dragDropFinishCbRec[0].closure=this;
    destroyCbRec[0].closure=this;
    
    Arg args[40];
    int n=0;
    XtSetArg(args[n],XmNexportTargets,_targets);n++;
    XtSetArg(args[n],XmNnumExportTargets,_numTargets);n++;
    XtSetArg(args[n],XmNclientData,(XtPointer)this);n++;
    XtSetArg(args[n],XmNconvertProc,DragConvertCb);n++;
    XtSetArg(args[n],XmNdragOperations,_operations);n++;
    XtSetArg(args[n],XmNblendModel,XmBLEND_JUST_SOURCE);n++;
    XtSetArg(args[n],XmNoperationChangedCallback,operationChangedCbRec);n++;
    XtSetArg(args[n],XmNdragMotionCallback,dragMotionCbRec);n++;
    XtSetArg(args[n],XmNdropStartCallback,dropStartCbRec);n++;
    XtSetArg(args[n],XmNdropSiteEnterCallback,dropSiteEnterCbRec);n++;
    XtSetArg(args[n],XmNdropSiteLeaveCallback,dropSiteLeaveCbRec);n++;
    XtSetArg(args[n],XmNdropFinishCallback,dropFinishCbRec);n++;
    XtSetArg(args[n],XmNdragDropFinishCallback,dragDropFinishCbRec);n++;
    XtSetArg(args[n],XmNdestroyCallback,destroyCbRec);n++;
    if (_dragIcon) {
#ifdef IRIX
#ifndef IRIX5_3
        // We want the cursor colors to be black and white, but
        // only IRIX Motif does the right thing with the color
        // settings. Other platforms spew many X protocol errors
        // from inside Xm/DragOverS.c in Motif.
        //
        // ifdef desired code for IRIX and let other platforms
        // use default colors until we can figure out a way
        // to stop the X protocol errors.
        Pixel fg,bg;

        // get a context
        MWContext *context=fe_WidgetToMWContext(_dragWidget);
        if (!context)
            context=XP_GetNonGridContext(fe_all_MWContexts->context);

        // extract color info
        if (context) {
            fg=fe_GetPixel(context,0,0,0);
            bg=fe_GetPixel(context,0xff,0xff,0xff);
        }
        else {
            XtVaGetValues(_dragWidget,XmNforeground,&fg,XmNbackground,&bg,NULL);
        }
        XtSetArg(args[n],XmNcursorBackground,bg);n++;
        XtSetArg(args[n],XmNcursorForeground,fg);n++;
#endif
#endif
        XtSetArg(args[n],XmNsourceCursorIcon,_dragIcon);n++;        
    }

#if 0
    // This seems fixed for now..  but I'm not deleting this code just yet,
    // since I don't know the who or the how of the fix.
    //
    // What a hack - Something in the browser menu code is causing
    // the menu system to think it's active each time a new browser
    // is created. This prevents any drags from working until
    // the first menu pops up, then down again, resetting the
    // 'userGrabbed' flag in XmDisplay.
    // We will reset that flag here to make dragging work. This
    // is safe, since we know that we only call XmDragStart() in
    // response to a button press - which will have already deactivated
    //  any menu code. Cool. Now as a warm-down send a manned mission
    // to Mars. Then find out why this happens in the first place!!!!
    Widget xmDisplay=XmGetXmDisplay(XtDisplay(_widget));
    XDEBUG(printf("XmDisplay.userGrabbed=%d\n",((XmDisplayRec*)xmDisplay)->display.userGrabbed));
    ((XmDisplayRec*)xmDisplay)->display.userGrabbed=0;
#endif
    
#ifndef DISABLE_DRAG
    // check that Btn1 is still down
    Window rootWin,childWin;
    int rootX,rootY,childX,childY;
    unsigned int buttonMask=0;

    XSync(XtDisplay(_widget),FALSE);
    XQueryPointer(XtDisplay(_widget),XtWindow(_widget),
                  &rootWin,&childWin,
                  &rootX,&rootY,&childX,&childY,
                  &buttonMask);
    if (!(buttonMask & Button1Mask)) {
        XDEBUG(printf("###Button1 not down - aborting drag!###\n"));
        dragDropFinishCb();
        return;
    }    
    
    XDEBUG(printf("XmDragStart() {\n"));
    _dragContext=XmDragStart(_dragWidget,(XEvent*)&_dragButtonEvent,args,n);
    XDEBUG(printf("}  XmDragStart()\n"));
    if (_dragContext) {
        _dragStarted=TRUE;
        // record global active drag shell
        Widget shell=_widget;
        while (!XtIsShell(shell)) shell=XtParent(shell);
        _activeDragShell=shell;
    }
    else {
        XDEBUG(printf("XmDragStart() failed!\n"));
        dragDropFinishCb();
        return;
    }    
#endif    
}

int XFE_DragBase::isFileURL(const char *url)
{
    return (url && strlen(url)>0 &&
            (XP_STRNCASECMP(url,"file:",5)==0 || XP_STRNCMP(url,"/",1)==0))
        ? TRUE : FALSE;
}

void XFE_DragBase::setDragIcon(struct fe_icon_data *iconData)
{
    // if this is a new icon, clean up old icon data
    if (_dragIconData!=iconData) {
        if (_dragIconPixmap!=None) {
            XFreePixmap(XtDisplay(_widget),_dragIconPixmap);
            _dragIconPixmap=None;
        }
        if (_dragIconPixmapMask!=None) {
            XFreePixmap(XtDisplay(_widget),_dragIconPixmapMask);
            _dragIconPixmapMask=None;
        }
    }

    // set up pointer, and let dragInitialize() create pixmaps
    _dragIconData=iconData;
}

void XFE_DragBase::setDragIconForType(const char *dataType)
{
    if (!dataType) {
        setDragIcon(&GUnknown);
        return;
    }
    
    if (!dataType || XP_STRCASECMP(dataType,UNKNOWN_CONTENT_TYPE)==0)
        setDragIcon(&GUnknown);
    else if (XP_STRCASECMP(dataType,"_url")==0)    // internal type name for urls
        setDragIcon(&LocationProxy);
    else if (XP_STRCASECMP(dataType,"_directory")==0)    // internal type name for directories
        setDragIcon(&GFolder);
    else if (XP_STRNCASECMP(dataType,"audio",5)==0)
        setDragIcon(&GAudio);
    else if (XP_STRNCASECMP(dataType,"application",11)==0)
        setDragIcon(&GBinary);
    else if (XP_STRNCASECMP(dataType,"image",5)==0)
        setDragIcon(&GImage);
#ifdef MOZ_MAIL_NEWS
    else if (XP_STRNCASECMP(dataType,"message",7)==0)
        setDragIcon(&MNTB_Forward);
    else if (XP_STRCASECMP(dataType,"text/x-vcard")==0)
        setDragIcon(&MNAB_NewPerson);
#endif
    else if (XP_STRNCASECMP(dataType,"text",4)==0)
        setDragIcon(&GText);
    else if (XP_STRNCASECMP(dataType,"video",5)==0)
        setDragIcon(&GMovie);
    else
        setDragIcon(&GUnknown);
}


//
// default drag virtual methods - derived class can override
//

// drag string data by default
void XFE_DragBase::targets()
{
    _numTargets=1;
    _targets=new Atom[_numTargets];

    _targets[0]=XA_STRING;
}

// allow move or copy by default
void XFE_DragBase::operations()
{
    _operations=(unsigned int)(XmDROP_MOVE | XmDROP_COPY);
}


// provide data for requested target from targets() list
char *XFE_DragBase::getTargetData(Atom target)
{
    // WARNING - data *must* be allocated with Xt malloc API, or Xt
    // will spring a leak!
    
    if (target==XA_STRING) {
        return (char*)XtNewString("Netscape 4.0 Default Drag Data");
    }

    return 0;
}


void XFE_DragBase::deleteTarget()
{
    XDEBUG(printf("XFE_DragBase::deleteTarget()\n"));
}

//
// button event handler methods
//

extern "C" void fe_HTMLDragSetLayer(CL_Layer *layer);

void XFE_DragBase::buttonPressCb(XButtonPressedEvent *event) 
{
    XDEBUG(printf("XFE_DragBase::buttonPressCb(%d,%d)\n",event->x,event->y));

    // Hack: Clear global HTMLView drag layer before button event is processed by XFE_HTMLView
    fe_HTMLDragSetLayer(NULL);

    // nyi - for now, don't lock out drags when Motif gets confused
    _dragStarted=FALSE;
    
    // one at a time please..
    if (_dragStarted)
        return;

    _buttonPressed=TRUE;
    _dragStartX=event->x;
    _dragStartY=event->y;
    _dragEventX=_dragStartX;
    _dragEventY=_dragStartY;
    
    // gross - copy button event for later use by XmDragStart
    // Note: Motif should allow Motion event in XmDragStart, docs
    // say Yes, but the Xm code disagrees - only ButtonPress is accepted.
    _dragButtonEvent=*event;

    // if there's no threshold, start drag immediately
    if (_dragThreshold==0) {
        dragInitialize();
        _buttonPressed=FALSE; // reset the trigger
    }

}

int XFE_DragBase::buttonMotionCb(XMotionEvent *event)
{
    // steal event if we're already dragging
    if (_dragStarted)
        return FALSE;

    // pass through event if we're not dragging 
    if (!_buttonPressed)
        return TRUE;
    
    XDEBUG(printf("XFE_DragBase::buttonMotionCb(%d,%d)\n",event->x,event->y));

    if (abs(_dragStartX-event->x)>=_dragThreshold ||
        abs(_dragStartY-event->y)>=_dragThreshold) {
        dragInitialize();
        _buttonPressed=FALSE; // reset the trigger
    }

    // steal event - user might be starting a drag
    return FALSE;
}

//
// drag callbacks
//

Boolean XFE_DragBase::dragConvertCb(Atom *target,Atom *type,XtPointer *value,
                                    unsigned long *length,int *format)
{
    XDEBUG(printf("XFE_DragBase::dragConvertCb(%s)\n",XmGetAtomName(XtDisplay(_widget),*target)));
    
    // TARGETS - our targets, plus ICCCM standard targets
    if (*target==_XA_TARGETS) {
        // create targets list - freed by Xt selection API
        Atom *targetData=(Atom*)XtMalloc((unsigned)((_numTargets+4)*sizeof(Atom)));
        int i;
        for(i=0;i<_numTargets;i++)
            targetData[i]=_targets[i];
        targetData[i++]=_XA_DELETE;
        targetData[i++]=_XA_TARGETS;
        targetData[i++]=_XA_MULTIPLE; // provided by Xt
        targetData[i++]=_XA_TIMESTAMP; // provided by Xt

        // return ICCCM targets list
        *value=(XtPointer)targetData;
        *type=XA_ATOM;
        *length=_numTargets+4;
        *format=32;
        return TRUE;
    }
    
    // DELETE
    if (*target==_XA_DELETE) {
        // handle delete request for dragged object
        deleteTarget();
        
        // return ICCCM acknowledgement of delete
        *value=NULL;
        *type=_XA_NULL;
        *length=0;
        *format=8;
        return TRUE;
    }

    // is it a request for the data as a desktop file?
    if (isFileTarget(*target)) {
        // write drag data in appropriate desktop file format, return filenames
        // DragBase does not handle files - methods are provided by derived class.
        // (but this is easier than adding yet another chained virtual method..)
        getTargetDataAsFileWrapper(*target,value,length);
        if (*value && *length>0) {
            if (*target==_XA_FILE_NAME)
                *type=XA_STRING;
            else
                *type=*target;
            *format=8;
            return TRUE;
        }
        else {
            return FALSE;
        }
    }
    
    // All other targets - ask derived class method for data string
    if ((*value=(XtPointer)getTargetData(*target))!=NULL) {
        *type=*target;
        *length=strlen((char*)*value);
        *format=8;
#if defined(DEBUG_tao)
		printf("\nXFE_DragBase::dragConvertCb:value=%x,target=%d,length=%d\n",
			   *value, *target, *length);
#endif

        return TRUE;
    }

    // Can't provide data for requested target
    return FALSE;
}

void XFE_DragBase::operationChangedCb()
{
    XDEBUG(printf("XFE_DragBase::operationChangedCb()\n"));
}

void XFE_DragBase::dragMotionCb()
{
    //XDEBUG(printf("XFE_DragBase::dragMotionCb()\n"));
}

void XFE_DragBase::dropStartCb()
{
    XDEBUG(printf("XFE_DragBase::dropStartCb()\n"));
}

void XFE_DragBase::dropSiteEnterCb(int operations)
{
    XDEBUG(printf("XFE_DragBase::dropSiteEnterCb()\n"));
    if (_dragFilesAsLinks && (operations & XmDROP_LINK)) {
        XtVaSetValues(_dragContext,XmNdragOperations,XmDROP_LINK,NULL);
    }
}

void XFE_DragBase::dropSiteLeaveCb()
{
    XDEBUG(printf("XFE_DragBase::dropSiteLeaveCb()\n"));
    if (_dragFilesAsLinks)
        XtVaSetValues(_dragContext,XmNdragOperations,_operations|XmDROP_LINK,NULL);
}

void XFE_DragBase::dropFinishCb()
{
    XDEBUG(printf("XFE_DragBase::dropFinishCb()\n"));
}

void XFE_DragBase::dragDropFinishCb()
{
    XDEBUG(printf("XFE_DragBase::dragDropFinishCb()\n"));

    dragComplete();

    // destroy drag icon widget
    if (_dragIcon) {
        XtDestroyWidget(_dragIcon);
        _dragIcon=NULL;
    }
    
    // delete targets list
    
    if (_targets) {
        delete [] _targets;
        _targets=NULL;
        _numTargets=0;
    }

    // reset drag data

    _operations=0;
    _dragWidget=NULL;
    _dragContext=NULL;
    _dragStarted=FALSE;
    _dragStartX=0;
    _dragStartY=0;
    _dragEventX=0;
    _dragEventY=0;
    _activeDragShell=NULL;
}



//
// XFE_DragDesktop
//
// app specifies dragging type via targets()
// if request comes in for file:
//      determine actual type from targets()
//      call getTargetData() for actual type
//      create tmp file in correct format
//      return SGI_ICON/SGI_FILE/FILENAME as needed
//      delete file when drop compete (assume force a file copy)
//      DELETE still means delete data in Netscape, not file

XFE_DragDesktop::XFE_DragDesktop(Widget w) : XFE_DragBase(w)
{
    _fileTarget=None;
}

XFE_DragDesktop::~XFE_DragDesktop()
{
}

void XFE_DragDesktop::setFileTarget(Atom target)
{
    if (!_targets || _numTargets==0)
        return;
    
    _fileTarget=target;
    
    // insert desktop file dnd atoms into list
    // must insert before generic STRING - some desktops (IRIX 6.3+)
    // will treat STRING drop as a paste-to-new-file action.
    Atom *newTargets=new Atom[_numTargets+4];
    int i=0;
    int ni=0;
    int added=FALSE;
    while (i<_numTargets) {
        if (_targets[i]==XA_STRING) {
            newTargets[ni++]=_XA_SGI_ICON_TYPE;
            newTargets[ni++]=_XA_SGI_ICON;
            newTargets[ni++]=_XA_SGI_FILE;
            newTargets[ni++]=_XA_FILE_NAME;
            added=TRUE;
        }
        newTargets[ni++]=_targets[i++];
    }
    if (!added) {
        // no STRING in targets list, so append
        newTargets[ni++]=_XA_SGI_ICON_TYPE;
        newTargets[ni++]=_XA_SGI_ICON;
        newTargets[ni++]=_XA_SGI_FILE;
        newTargets[ni++]=_XA_FILE_NAME;
    }

    delete [] _targets;
    _targets=newTargets;
    _numTargets=ni;
}


int XFE_DragDesktop::isFileTarget(Atom target)
{
    return (target==_XA_SGI_ICON ||
            target==_XA_SGI_ICON_TYPE ||
            target==_XA_SGI_FILE ||
            target==_XA_FILE_NAME)
        ? TRUE : FALSE;
}

void XFE_DragDesktop::getTargetDataAsFileWrapper(Atom target, XtPointer *value, unsigned long *length)
{
    // create files for data and return desktop formatted description of files.

    *value=NULL;
    *length=0;

    XDEBUG(printf("XFE_DragDesktop::getTargetDataAsFileWrapper(%s)\n",XmGetAtomName(XtDisplay(_widget),target)));
    
    if (_fileTarget==None)
        return;

    // return type for desktop feedback during drag
    if (target==_XA_SGI_ICON_TYPE) {
        const char *ftrType=getTargetFTRType(_fileTarget);
        if (ftrType) {
            *value=(XtPointer)XtNewString(ftrType);
            *length=strlen(ftrType);
        }
        else {
            *value=NULL;
            *length=0;
        }
        return;
    }

    // get data as list of files
    // (usually one file per selected item, but it's up to derived class)
    char **fileList=NULL;
    int numFiles=0;
    getTargetDataAsFileList(_fileTarget,&fileList,&numFiles);
    
    if (!fileList || numFiles==0)
        return;

    const char *ftrType=getTargetFTRType(_fileTarget);
    const char SGI_ICON_FORMAT[] =
        "Category:%s Name:%s Type:%s Host:%s ViewX:%d ViewY:%d ViewGallery:%d";
    int i;
    int fileDataSize=0;
    // calculate space needed for formatted file list
    for (i=0;i<numFiles;i++)
        if (fileList[i]) fileDataSize+=strlen(fileList[i])+1;
    if (target==_XA_SGI_ICON)
        fileDataSize+=numFiles*(strlen(SGI_ICON_FORMAT)+
                                strlen(ftrType)+
                                strlen(localHostName)+
                                50); // format string space plus ViewX,ViewY,ViewGallery + final \0
    else
        fileDataSize+=1; // + final \0

    // copy file list to value in appropriate format
    char *fileData=XtMalloc(fileDataSize);
    unsigned long fileDataLength=0;
    char *curPos=fileData;
    for (i=0;i<numFiles;i++) {
        if (target==_XA_SGI_ICON) {
            // nyi - tune values for ViewX,ViewY s.t. drop of icon looks right
            // nyi - do we need to tweak ViewX,ViewY for multiple item drops?  (orientation,spacing)
            sprintf(curPos,SGI_ICON_FORMAT,"File",fileList[i],ftrType,localHostName,
                    -10,-10,0);
        }
        else {
            strcpy(curPos,fileList[i]);
        }
        int segmentLen=strlen(curPos)+1;
        fileDataLength+=segmentLen;
        curPos+=segmentLen;
    }
    *curPos='\0'; // terminate with extra \0
    fileDataLength++;

    // free file list
    for (i=0;i<numFiles;i++)
        if (fileList[i]) XP_FREE(fileList[i]);
    delete fileList;
    
    *value=(XtPointer)fileData;
    *length=fileDataLength;
}


void XFE_DragDesktop::getTargetDataAsFileList(Atom,char ***fileList,int *numFiles)
{
    *fileList=NULL;
    *numFiles=0;
    return;
}

const char *XFE_DragDesktop::getTargetFTRType(Atom)
{
    return DEFAULT_FTR_TYPE;
}

void XFE_DragDesktop::dragDropFinishCb()
{
    XDEBUG(printf("XFE_DragDesktop::dragDropFinishCb()\n"));
    
    XFE_DragBase::dragDropFinishCb();

    // reset drag data
    _fileTarget=None;    
}


//
// XFE_DragNetscape
//      - provide support for all Netsape drag types
// and their conversion to a desktop file format. No actual
// drag targets are specified by this class. Derived class
// must specify targets(), operations() and getTargetData()
//

XFE_DragNetscape::XFE_DragNetscape(Widget w) : XFE_DragDesktop(w)
{
    _desktopFileData=NULL;
}

XFE_DragNetscape::~XFE_DragNetscape()
{
    if (_desktopFileData) {
        _desktopFileData->cleanupDataFiles();
        delete _desktopFileData;
        _desktopFileData=NULL;
    }
}


void XFE_DragNetscape::getTargetDataAsFileList(Atom target,char ***fileList,int *numFiles)
{
    *numFiles=0;
    *fileList=NULL;

    if (_desktopFileData!=NULL) {
        // make sure we only have one set of tmp files per drag
        // (dumb or malicious drop site might ask for FILE_NAME and _SGI_ICON)
        return;
    }
    
    XDEBUG(printf("XFE_DragNetscape::getTargetDataAsFileList(%s)\n",XmGetAtomName(XtDisplay(_widget),target)));
           
    // XA_STRING - interpret data as name of existing file
    if (target==XA_STRING) {
        char *filename=getTargetData(XA_STRING);
        if (filename) {
            *numFiles=1;
            *fileList=new char*[1];
            (*fileList)[0]=filename;
        }
        return;
    }

    // XA_FILE_NAME - interpret data as name of existing file
    if (target==_XA_FILE_NAME) {
        char *filename=getTargetData(_XA_FILE_NAME);
        if (filename) {
            *numFiles=1;
            *fileList=new char*[1];
            (*fileList)[0]=filename;
        }
        return;
    }

    // _XA_NETSCAPE_URL - translate data into NetscapeURL desktop file(s)
    if (target==_XA_NETSCAPE_URL) {
        char *stringData=getTargetData(_XA_NETSCAPE_URL);
        if (!stringData)
            return;

        XFE_URLDesktopType *urlData=new XFE_URLDesktopType(stringData);
        *numFiles=urlData->numItems();
        if (*numFiles==0) {
            delete urlData;
            XtFree(stringData);
            return;
        }
        

        // write files to tmp directory
        urlData->writeDataFiles(_dragFilesAsLinks);
        

        // extract filenames of tmp files for drag response
        *fileList=new char*[*numFiles];
        int i;
        for (i=0;i<*numFiles;i++) {
            if (urlData->filename(i))
                (*fileList)[i]=XP_STRDUP(urlData->filename(i));
            else
                (*fileList)[i]=NULL;
        }
        
        // save desktop tmp file data  for cleanup at end of drag
        _desktopFileData=(XFE_DesktopType*)urlData;

        XtFree(stringData);
    }

    return;
}

const char *XFE_DragNetscape::getTargetFTRType(Atom target)
{
    if (target==_XA_NETSCAPE_URL) return XFE_URLDesktopType::ftrType();
    
    return DEFAULT_FTR_TYPE;
}


void XFE_DragNetscape::dragDropFinishCb()
{
    XFE_DragDesktop::dragDropFinishCb();

    // cleanup temporary directory and files used for Netscape data
    if (_desktopFileData) {
        _desktopFileData->cleanupDataFiles();
        delete _desktopFileData;
        _desktopFileData=NULL;
    }
}


//
// XFE_DropBase
//

//
// callback stubs
//

void XFE_DropBase::DragProc(Widget w, XtPointer,XtPointer cb)
{
    XtPointer ud;
    XtVaGetValues(w,XmNuserData,&ud,NULL);
    XFE_DropBase *d=(XFE_DropBase*)ud;
    if (d)
        d->dragProc((XmDragProcCallbackStruct*)cb);
}


void XFE_DropBase::DropProc(Widget w,XtPointer,XtPointer cb) {
    XtPointer ud;
    XtVaGetValues(w,XmNuserData,&ud,NULL);
    XFE_DropBase *d=(XFE_DropBase*)ud;
    if (d)
        d->dropProc((XmDropProcCallbackStruct*)cb);
}

void XFE_DropBase::DestroyCb(Widget,XtPointer cd,XtPointer) {
    XFE_DropBase *d=(XFE_DropBase*)cd;
    if (d)
        d->dropComplete();
}

void XFE_DropBase::TransferProc(Widget w, XtPointer cd, Atom */*seltype*/, Atom *type,
                            XtPointer value, unsigned long *length, int */*format*/)
{
#if defined(DEBUG_tao)
	printf("\nXFE_DropBase::TransferProc, value=%x\n", value);
#endif
    XFE_DropBase *d=(XFE_DropBase*)cd;
    if (d)
        d->transferProc(w,*type,value,(unsigned int)*length);
}


// constructor

XFE_DropBase::XFE_DropBase(Widget widget)
{
    _widget=widget;
    _operations=(unsigned int)XmDROP_NOOP;
    _targets=NULL;
    _numTargets=0;
    _dropEventX=0;
    _dropEventY=0;
    _chosenTarget=None;

    InitializeDisplayInfo(widget);
    
    // create drop site, disabled by default
#ifdef DROP_ENABLED    
    Arg args[20];
    int n=0;
    XtSetArg(args[n],XmNdragProc,DragProc); n++;
    XtSetArg(args[n],XmNdropProc,DropProc); n++;
    XtSetArg(args[n],XmNdropSiteActivity,XmDROP_SITE_INACTIVE);n++;
    XtSetArg(args[n],XmNdropSiteType,XmDROP_SITE_COMPOSITE);n++;
    
    XmDropSiteRegister(_widget, args, n);
#endif
    // make XFE_DropBase available to DragProc, DropProc
    XtVaSetValues(widget,XmNuserData,this,NULL);

    // initialize destroy callback struct
    _destroyCbList[0].callback=DestroyCb;
    _destroyCbList[0].closure=(XtPointer)this;
    _destroyCbList[1].callback=NULL;
    _destroyCbList[1].closure=NULL;
}


// destructor

XFE_DropBase::~XFE_DropBase()
{
#ifdef DROP_ENABLED
	//
	//    Don't do this, because XmDropSiteRegister() sets up a destroy
	//    callback that does this (well the equivelent). If it's called twice,
	//    we crash in _XmIEndUpdate()....djw
#if 0
    if (_widget)
        XmDropSiteUnregister(_widget);
#endif
#endif
    freeTargetList();
}

// convenience fn - allow easy cleanup of old targets

void XFE_DropBase::freeTargetList()
{
    if (_targets) {
        delete [] _targets;
        _targets=NULL;
    }
    _numTargets=0;
}


// drop site management

void XFE_DropBase::enable()
{
    // enable site
#ifdef DROP_ENABLED    
    Arg args[1];
    XtSetArg(args[0],XmNdropSiteActivity,XmDROP_SITE_ACTIVE);
    XmDropSiteUpdate(_widget,args,1);
#endif
    // set targets and operations
    // (can't do it in constructor because targets() and operations() are virtual)
    update();
}


void XFE_DropBase::disable()
{
#ifdef DROP_ENABLED    
    Arg args[1];
    XtSetArg(args[0],XmNdropSiteActivity,XmDROP_SITE_INACTIVE);
    XmDropSiteUpdate(_widget,args,1);
#endif    
}

void XFE_DropBase::update()
{
    // allow derived class to change attributes
    operations();
    freeTargetList();
    targets();
    
    // apply changes to drop site
#ifdef DROP_ENABLED    
    Arg args[20];
    int n=0;
    XtSetArg(args[n], XmNimportTargets, _targets); n++;
    XtSetArg(args[n], XmNnumImportTargets, _numTargets); n++;
    XtSetArg(args[n], XmNdropSiteOperations, _operations); n++;
    XmDropSiteUpdate(_widget,args,n);
#endif    
}

void XFE_DropBase::update(ArgList args,Cardinal n)
{
    // apply target/operation changes to drop site
	update();

    // apply user defined changes to drop site
#ifdef DROP_ENABLED    
    XmDropSiteUpdate(_widget,args,n);
#endif    

}

// override to set _operations

void XFE_DropBase::operations()
{
    _operations=(unsigned int) (XmDROP_MOVE | XmDROP_COPY);
}

// override to set _targets

void XFE_DropBase::targets()
{
    freeTargetList();

    _numTargets=1;
    _targets=new Atom[_numTargets];

    _targets[0]=XA_STRING;
}

// override to add drag-in feature
void XFE_DropBase::dragIn()
{
    XDEBUG(printf("XFE_DropBase::dragIn(%d,%d)\n",_dropEventX,_dropEventY));
}

// override to add drag-out feature

void XFE_DropBase::dragOut()
{
    XDEBUG(printf("XFE_DropBase::dragOut(%d,%d)\n",_dropEventX,_dropEventY));
}

// override to add drag-motion feature
void XFE_DropBase::dragMotion()
{
    // XDEBUG(printf("XFE_DropBase::dragMotion(%d,%d)\n",_dropEventX,_dropEventY));
}

// override to add operation-changed feature

void XFE_DropBase::operationChanged()
{
}

// override to add validation of drop targets

Atom XFE_DropBase::acceptDrop(unsigned int dropOperation,Atom *dropTargets,unsigned int numDropTargets)
{
    // pick first matching target in class's list, if operation is valid
    
    if (!(dropOperation & _operations))
        return None;

    Atom selected=None;
    
    XDEBUG(printf("TARGETS:\n"));
#if defined(DEBUG_tao)
	printf("\nXFE_DropBase::acceptDrop\n");
	for (int ii=0; ii < _numTargets; ii++)
		printf("\n_targets[%d]=%s", 
			   ii, XGetAtomName(XtDisplay(_widget),_targets[ii]));
	for (int jj=0; jj < numDropTargets; jj++)
		printf("\ndropTargets[%d]=%s", 
			   jj, XGetAtomName(XtDisplay(_widget),dropTargets[jj]));
#endif
    for (int i=0;i<_numTargets; i++) {
        for (int j=0; j < (int)numDropTargets; j++) {
             XDEBUG(if (i==0) printf("  %s\n",XGetAtomName(XtDisplay(_widget),dropTargets[j])));
            if (_targets[i]==dropTargets[j]) {
                selected=dropTargets[j];
                break;
            }
        }
        if (selected!=None)
            break;
    }

    return selected;
}


// override to tidy up after drop

void XFE_DropBase::dropComplete()
{
}

// callback methods

void XFE_DropBase::dragProc(XmDragProcCallbackStruct *cb)
{
    // record location of drop:
    _dropEventX=cb->x;
    _dropEventY=cb->y;

    switch(cb->reason) {
        case XmCR_DROP_SITE_ENTER_MESSAGE:
            // verify that this combo of operation and targets is acceptable
            if (cb->operation!=XmDROP_NOOP &&
                acceptDropWrapper(cb->operation,cb->dragContext)!=None) {
                cb->dropSiteStatus = XmVALID_DROP_SITE;
                dragIn();
            }
            else {
                cb->dropSiteStatus = XmINVALID_DROP_SITE;
            }
            break;
        case XmCR_DROP_SITE_LEAVE_MESSAGE:
            dragOut();
            break;
        case XmCR_DROP_SITE_MOTION_MESSAGE:
            dragMotion();
            break;
        case XmCR_OPERATION_CHANGED:
            operationChanged();
            break;
        default:
            cb->dropSiteStatus = XmINVALID_DROP_SITE;
            break;
    }

    // allow animation to be performed
    cb->animate = True;
}



void XFE_DropBase::dropProc(XmDropProcCallbackStruct *cb)
{
    if (cb->dropAction != XmDROP_HELP) {
        handleDrop(cb);
    }
    else {
        // option to display help dialog - we just cancel drag
        Arg args[10];
        int n=0;
        cb->dropSiteStatus=XmINVALID_DROP_SITE;
        XtSetArg(args[n],XmNtransferStatus,XmTRANSFER_FAILURE); n++;
        XtSetArg(args[n],XmNnumDropTransfers,0); n++;
        XmDropTransferStart(cb->dragContext,args,n);
        return;        
    }
}

void XFE_DropBase::handleDrop(XmDropProcCallbackStruct *cb)
{
    Arg args[10];
    int n;

    // record location of drop:
    _dropEventX=cb->x;
    _dropEventY=cb->y;
    
    // Cancel the drop on invalid drop operations

    if (cb->operation==XmDROP_NOOP) {
        n = 0;
        cb->dropSiteStatus = XmINVALID_DROP_SITE;
        XtSetArg(args[n], XmNtransferStatus, XmTRANSFER_FAILURE); n++;
        XtSetArg(args[n], XmNnumDropTransfers, 0); n++;
        XmDropTransferStart(cb->dragContext, args, n);
        return;
    }

    // select target based on operation and available targets
    // (remember chosen target for use later in tranfer proc. naughty
    // CDE file mgr doesn't give the target it was asked for.)
    
    _chosenTarget=acceptDropWrapper(cb->operation,cb->dragContext);
    
    if (_chosenTarget==None) { // transfer not valid
        n = 0;
        cb->operation = (unsigned char)XmDROP_NOOP;
        cb->dropSiteStatus = XmINVALID_DROP_SITE;
        XtSetArg(args[n], XmNtransferStatus, XmTRANSFER_FAILURE); n++;
        XtSetArg(args[n], XmNnumDropTransfers, 0); n++;
        XmDropTransferStart(cb->dragContext, args, n);
        return;
    }
    
    // start transfer

    XmDropTransferEntryRec transfers[2];
    Cardinal numTransfers = 1;
    transfers[0].target = _chosenTarget;
    transfers[0].client_data=(XtPointer)this;

    // send DELETE to drag source if this is a move
    if (cb->operation == XmDROP_MOVE) {
        transfers[1].target=_XA_DELETE;
        transfers[1].client_data=(XtPointer)this;
        numTransfers+=1;
    }

        
    n = 0;
    cb->dropSiteStatus = XmVALID_DROP_SITE;
    XtSetArg(args[n], XmNdropTransfers, transfers); n++;
    XtSetArg(args[n], XmNnumDropTransfers, numTransfers); n++;
    XtSetArg(args[n], XmNdestroyCallback, _destroyCbList); n++;
    XtSetArg(args[n], XmNtransferProc, TransferProc); n++;
    XmDropTransferStart(cb->dragContext, args, n);
}


void XFE_DropBase::transferProc(Widget dragContext,Atom type, XtPointer value,unsigned int length)
{
    // transfer data via dropTransfer()

    XDEBUG(printf("XFE_DropBase::transferProc(%s)\n",type ? XmGetAtomName(XtDisplay(_widget),type):"(null)"));

    // check for response to a DELETE message - no action required
    if (type==None || type==_XA_NULL) {
        return;
    }
    
    if (value && length>0) {
        if (isFileTarget(_chosenTarget) &&
            (type==_chosenTarget || type==XA_STRING)) {
            // process file targets in XFE_DropDesktop
            // (test for XA_STRING is a hack to deal with CDE 1.0 bug, where
            // dtfile returns STRING data in response to request for FILE_NAME.)
            if (processFileTargetWrapper(_chosenTarget,value,length))
                return; // success
        }
        else {
            // package up item into drop list and pass to derived class method
            Atom *targets=new Atom[1];
            const char **dropData=new const char*[1];
            char *dropInfo=new char[length+1];
            memcpy(dropInfo,(char*)value,length);
            dropInfo[length]='\0';
            targets[0]=type;
            dropData[0]=dropInfo;
            int dropStatus=processTargets(targets,dropData,1);
            delete [] targets;
            delete dropData;
            delete dropInfo;

            if (dropStatus)
                return; // success
        }    
    }
    else {
        // transfer failed
        XtVaSetValues(dragContext,
                      XmNtransferStatus, XmTRANSFER_FAILURE,
                      XmNnumDropTransfers, 0,
                      NULL);
    }
    
    // nyi - is this right? doesn't Xt/Xm free this?, why not free above?
    if (value)
        XtFree((char*)value);
}

// wrapper around virtual function acceptDrop()
// (hide Motif cruft from dervived class)

Atom XFE_DropBase::acceptDropWrapper(unsigned int op,Widget dragContext)
{
    Atom *exportTargets=NULL;
    Cardinal numExportTargets=0;
    XtVaGetValues(dragContext,
                  XmNexportTargets, &exportTargets,
                  XmNnumExportTargets, &numExportTargets,
                  NULL);
    if (exportTargets && numExportTargets>0)
        return acceptDrop(op,exportTargets,numExportTargets);
    else
        return None;
}


//
// XFE_DropDesktop
//

// constructor

XFE_DropDesktop::XFE_DropDesktop(Widget widget) : XFE_DropBase(widget)
{
}


// destructor

XFE_DropDesktop::~XFE_DropDesktop()
{
}

//
// methods below override XFE_DropBase defaults
//

// desktop file types we understand
int XFE_DropDesktop::isFileTarget(Atom target)
{
    return (target==_XA_SGI_ICON ||
            target==_XA_SGI_FILE ||
            target==_XA_SGI_URL ||
            target==_XA_FILE_NAME)
        ? TRUE : FALSE;
}

void XFE_DropDesktop::acceptFileTargets()
{
    if (!_targets || _numTargets==0)
        return;

    // insert desktop file dnd atoms into list
    // must insert before generic STRING - some desktops (IRIX 6.3+)
    // will provide filename or URL as STRING - we want all the info
    Atom *newTargets=new Atom[_numTargets+5];
    int i=0;
    int ni=0;
    int added=FALSE;
    while (i<_numTargets) {
        if (_targets[i]==XA_STRING) {
            newTargets[ni++]=_XA_SGI_URL;
            newTargets[ni++]=_XA_SGI_ICON;
            newTargets[ni++]=_XA_SGI_FILE;
            newTargets[ni++]=_XA_FILE_NAME;
            newTargets[ni++]=_XA_TARGETS; // debugging hack - force display of all targets
            added=TRUE;
        }
        newTargets[ni++]=_targets[i++];
    }
    if (!added) {
        // no STRING in targets list, so append
        newTargets[ni++]=_XA_SGI_URL;
        newTargets[ni++]=_XA_SGI_ICON;
        newTargets[ni++]=_XA_SGI_FILE;
        newTargets[ni++]=_XA_FILE_NAME;
        newTargets[ni++]=_XA_TARGETS; // debugging hack - force display of all targets
    }
    
    delete [] _targets;
    _targets=newTargets;
    _numTargets=ni;
}

// process dropped data

int XFE_DropDesktop::processFileTargetWrapper(Atom type,XtPointer value,unsigned int length)
{
    if (!value || length==0)
        return FALSE;

    int dropStatus=FALSE;
    
    char *dropInfo=new char[length+1];
    memcpy(dropInfo,(char*)value,length);
    dropInfo[length]='\0';
    
    if (type==_XA_SGI_URL)
        dropStatus=processSGI_URL(dropInfo,length);
    else if (type==_XA_SGI_FILE)
        dropStatus=processFILE_NAME(dropInfo,length);
    else if (type==_XA_SGI_ICON)
        dropStatus=processSGI_ICON(dropInfo,length);
    else if (type==_XA_FILE_NAME)
        dropStatus=processFILE_NAME(dropInfo,length);

    delete dropInfo;
    
    return dropStatus;
}


int XFE_DropDesktop::processSGI_URL(char *dropInfo,unsigned int /*length*/)
{
    // parse url and title out of SGI_URL drop info
    
    char *url=NULL;
    char *title=NULL;

    // url
    char *urlStart=strstr(dropInfo,"SRC=\"");
    if (urlStart) {
        urlStart+=5;
        char *end=urlStart;
        while (*end && *end!='\"') // look for closing quotes
            end++;
        int len=end-urlStart;
        if (len>0) {
            url=new char[len+1];
            strncpy(url,urlStart,len);
            url[len]='\0';
        }
    }

    // title
    char *titleStart=strstr(dropInfo,"TITLE=\"");
    if (titleStart) {
        titleStart+=7;
        char *end=titleStart;
        while (*end && !(*end=='\"' && *(end-1)!='\\')) // look for non-escaped closing quotes
            end++;
        int len=end-titleStart;
        if (len>0) {
            title=new char[len+1];
            strncpy(title,titleStart,len);
            title[len]='\0';
        }
    }

    // should this be a list, separated by '\0'?
    int dropStatus=processDesktopURLTarget(title,url);
    
    if (url)
        delete url;
    if (title)
        delete title;

    return dropStatus;
}


int XFE_DropDesktop::processSGI_ICON(char *dropInfo,unsigned int length)
{
    const int FTR_MAX_SIZE=128; // from ftr header files
    const char SGI_ICON_FORMAT[] =
        "Category:%s Name:%s Type:%s Host:%s ViewX:%d ViewY:%d ViewGallery:%d";
    
    const int MAX_FILES=200;
    char *files[MAX_FILES];
    int numFiles=0;

    // parse individual files out of _SGI_ICON drop info
    
    char *start=dropInfo;;
    while (start<dropInfo+length && numFiles<MAX_FILES) {
        if (strlen(start)>0) {

            // parse _SGI_ICON data using standard format
            char filename[MAXPATHLEN]="";
            char ftrname[FTR_MAX_SIZE]="";
            char category[FTR_MAX_SIZE]="";
            char hostName[MAXHOSTNAMELEN]="";
            int viewx=0, viewy=0, gal=0;
            
            int numFields=sscanf(start, SGI_ICON_FORMAT,
                               category, filename, ftrname,
                               hostName, &viewx, &viewy, &gal);
            
            // ignore files located on remote hosts
            // (test strips off domain name, uses only host name)
            char *dot;
            if (dot=strchr(hostName,'.'))
                *dot='\0';
            if (strcmp(hostName,localHostName)==0) {
                // copy into file list
                files[numFiles++]=strdup(filename);
            }
        }
        
        start+=strlen(start)+1;         // move to next file     
    }

    int dropStatus=processFileTarget((const char**)files,numFiles);

    // free file name strings and list
    for (int i=0;i<numFiles;i++)
        if (files[i]) free((void*)files[i]);

    return dropStatus;
}

int XFE_DropDesktop::processFILE_NAME(char *dropInfo,unsigned int length)
{
    const int MAX_FILES=200;
    char *files[MAX_FILES];
    int numFiles=0;
    int dropStatus=FALSE;

    // parse individual files out of drop info

    char *start=dropInfo;;
    while (start<dropInfo+length && numFiles<MAX_FILES) {
        // detect end of chunk, separated by null
        char *end=start;
    	while(*end!='\0' && end<dropInfo+length)
    	    end++;
        *end='\0';  // ensure this chunk is terminated
        
        if (strlen(start)>0) {
            if (NET_URL_Type(start)==0) {
                files[numFiles++]=start;       // add to file drop list
            }
        }
        
        start=end+1;     
    }
    
    if (processFileTarget((const char**)files,numFiles))
        dropStatus=TRUE;
    
    return dropStatus;
}

//
// XFE_DropNetscape
//

// constructor

XFE_DropNetscape::XFE_DropNetscape(Widget widget) : XFE_DropDesktop(widget)
{
}


// destructor

XFE_DropNetscape::~XFE_DropNetscape()
{
}

//
// methods below override XFE_DropDesktop defaults
//

int XFE_DropNetscape::processFileTarget(const char**files,int numFiles)
{
    XDEBUG(printf("XFE_DropNetscape::processFileTarget()\n"));

    Atom *targets=new Atom[numFiles];
    const char **data=new const char*[numFiles];
    const int MAX_LENGTH=4000;
    char line[MAX_LENGTH+1];
    line[MAX_LENGTH]='\0';

    int i;

    // build list of dropped items
    for (i=0;i<numFiles;i++) {
        // default is regular file
        targets[i]=_XA_FILE_NAME;
        data[i]=files[i];
        
        // if it's a Netscape desktop file, extract the data
        FILE *fp;
        if (fp=fopen(files[i],"r")) {
            line[0]='\0'; // ensure string will be null-terminated
            fgets(line,MAX_LENGTH,fp);

            // NetscapeURL
            if (XFE_URLDesktopType::isDesktopType(line)) {
                targets[i]=_XA_NETSCAPE_URL;
                XFE_URLDesktopType urlData(fp);
                const char *str=urlData.getString();    
                data[i]=(str ? XP_STRDUP(str) : 0);
            }
            
            // nyi - check for other Netscape desktop types here
            // (are no others..  yet.)
            fclose(fp);
        }
    }

    int dropStatus=processTargets(targets,data,numFiles);

    // free our copy of non-file data (FILE_NAME's were not copied)
    for (i=0;i<numFiles;i++) {
        if (targets[i]!=_XA_FILE_NAME)
            if (data[i])
                XP_FREE((void*)data[i]);
    }
    
    delete [] targets;
    delete data;
    
    return dropStatus;
}

//
// translate desktop URL type into a NetscapeURL
//
int XFE_DropNetscape::processDesktopURLTarget(const char *title,const char *url)
{
    XDEBUG(printf("XFE_DropNetscape::processDesktopURLTarget(%s,%s)\n",title,url));

    if (!url)
        return FALSE;
    
    Atom targets[1];
    const char *data[1];

    targets[0]=_XA_NETSCAPE_URL;
    XFE_URLDesktopType urlData;
    urlData.createItemList(1);
    urlData.url(0,url);
    if (title) urlData.title(0,title);
    
    const char *str=urlData.getString();
    if (str) {
        data[0]=str;
        int dropStatus=processTargets(targets,data,1);
	// nyi - when we add more data, do cleanup here.
        return dropStatus;
    }
    else {
        return FALSE;
    }

    // nyi - delete data and/or contents?
}


