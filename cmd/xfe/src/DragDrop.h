/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   DragDrop.h -- class definitions for drag and drop
   Created: Alastair Gourlay(SGI) c/o Dora Hsu<dora@netscape.com>, 26 Nov 1996
 */



#ifndef _DRAG_DROP_H
#define _DRAG_DROP_H

// Classes in this file:
//      XFE_DragBase
//        XFE_DragDesktop
//          XFE_DragNetscape
//      XFE_DropBase
//        XFE_DropDesktop
//          XFE_DropNetscape
//


#include <stdlib.h>
#include <Xm/XmAll.h>
#include <icondata.h>
#include "DesktopTypes.h"

class XFE_DragBase
{
public:
    XFE_DragBase(Widget);
    virtual ~XFE_DragBase();
    void dragThreshold(unsigned int t) { _dragThreshold=t; };
    unsigned int dragThreshold() { return _dragThreshold; };
    void addDragWidget(Widget);
    static void dragWidgetDestroyCb(Widget,XtPointer,XtPointer);
    
    // global drag information
    static Widget _activeDragShell;

    // globally useful method
    static char *guessUrlMimeType(const char*);
    
    static Atom _XA_TARGETS;
    static Atom _XA_DELETE;
    static Atom _XA_MULTIPLE;
    static Atom _XA_TIMESTAMP;
    static Atom _XA_NULL;
    static Atom _XA_SGI_ICON;
    static Atom _XA_SGI_ICON_TYPE;
    static Atom _XA_SGI_FILE;
    static Atom _XA_FILE_NAME;
    static Atom _XA_NETSCAPE_URL;
protected:
    Widget _widget;
    unsigned int _dragThreshold;
    int _dragStarted;
    int _buttonPressed;
    int _dragStartX;
    int _dragStartY;
    int _dragEventX;
    int _dragEventY;
    int _dragFilesAsLinks;
    Widget _dragWidget;
    Widget _dragContext;
    Widget _dragIcon;
    XButtonPressedEvent _dragButtonEvent;

    Atom *_targets;
    int _numTargets;
    unsigned int _operations;
    struct fe_icon_data *_dragIconData;
    Pixmap _dragIconPixmap;
    Pixmap _dragIconPixmapMask;
    unsigned int _dragIconWidth;
    unsigned int _dragIconHeight;
    unsigned int _dragHotX;
    unsigned int _dragHotY;

    // drag methods
    void dragInitialize();
    void dragFilesAsLinks(int d) { _dragFilesAsLinks=d; };
    static int isFileURL(const char*);
    void setDragIcon(struct fe_icon_data*);
    void setDragIconForType(const char*);
    
    // methods for derived classes to override
    virtual int dragStart(int,int) { return TRUE; };
    virtual void targets();
    virtual void operations();
    virtual char *getTargetData(Atom);
    virtual void deleteTarget();
    virtual int isFileTarget(Atom) { return FALSE; };
    virtual void getTargetDataAsFileWrapper(Atom,XtPointer*,unsigned long*) { };
    virtual void dragComplete() { };

    // callback wrappers
    static void ButtonPressCb(Widget,XtPointer,XEvent*,Boolean*);
    static void ButtonMotionCb(Widget,XtPointer,XEvent*,Boolean*);
    static Boolean DragConvertCb(Widget,Atom*,Atom*,Atom*,XtPointer*,unsigned long*,int*);
    static void OperationChangedCb(Widget,XtPointer,XtPointer);
    static void DragMotionCb(Widget,XtPointer,XtPointer);
    static void DropStartCb(Widget,XtPointer,XtPointer);
    static void DropSiteEnterCb(Widget,XtPointer,XtPointer);
    static void DropSiteLeaveCb(Widget,XtPointer,XtPointer);
    static void DropFinishCb(Widget,XtPointer,XtPointer);
    static void DragDropFinishCb(Widget,XtPointer,XtPointer);
    static void DestroyCb(Widget,XtPointer,XtPointer);

    // callback methods
    virtual void buttonPressCb(XButtonPressedEvent*);
    int buttonMotionCb(XMotionEvent*);
    Boolean dragConvertCb(Atom*,Atom*,XtPointer*,unsigned long*,int*);
    void operationChangedCb();
    void dragMotionCb();
    void dropStartCb();
    void dropSiteEnterCb(int);
    void dropSiteLeaveCb();
    void dropFinishCb();
    virtual void dragDropFinishCb();
    
private:
};

class XFE_DragDesktop : public XFE_DragBase 
{
public:
    XFE_DragDesktop(Widget);
    virtual ~XFE_DragDesktop();
protected:
    Atom _fileTarget;

    // file handler methods    
    void setFileTarget(Atom);
    void getTargetDataAsFileWrapper(Atom,XtPointer*,unsigned long*);
    virtual void dragDropFinishCb();

    // methods for derived classes to override
    virtual int isFileTarget(Atom);
    virtual void getTargetDataAsFileList(Atom,char ***,int *);
    virtual const char *getTargetFTRType(Atom);
private:
};

class XFE_DragNetscape : public XFE_DragDesktop 
{
public:
    XFE_DragNetscape(Widget);
    virtual ~XFE_DragNetscape();
protected:
    XFE_DesktopType *_desktopFileData;

    // drag methods
    virtual void dragDropFinishCb();
    
    // methods for derived classes to override
    virtual void getTargetDataAsFileList(Atom,char ***,int *);
    virtual const char *getTargetFTRType(Atom);
private:
};

//
// XFE_DropBase
//

class XFE_DropBase
{
public:
    XFE_DropBase(Widget);
    virtual ~XFE_DropBase();
    void enable();
    void disable();
    void update();
    void update(ArgList,Cardinal);
    
    static Atom _XA_NULL;
    static Atom _XA_DELETE;
    static Atom _XA_SGI_FILE;
    static Atom _XA_SGI_ICON;
    static Atom _XA_SGI_URL;
    static Atom _XA_FILE_NAME;
    static Atom _XA_TARGETS;
    static Atom _XA_NETSCAPE_URL;
protected:
    Widget _widget;
    unsigned int _operations;
    Atom *_targets;
    int _numTargets;
    XtCallbackRec _destroyCbList[2];
    Atom _chosenTarget;

    int _dropEventX;
    int _dropEventY;
    
    // methods available to derived class
    virtual void operations(); // override to set _operations
    virtual void targets(); // override to set _targets
    virtual void dragIn(); // override to add drag-in feature
    virtual void dragOut(); // override to add drag-out feature
    virtual void dragMotion(); // override to add drag-motion feature
    virtual void operationChanged(); // override to add operation-changed feature
    virtual Atom acceptDrop(unsigned int,Atom*,unsigned int); // override to validate operation and targets
    virtual void dropComplete(); // override to tidy up after drop
    virtual void freeTargetList(); // convenience fn - allow easy cleanup of old targets
    virtual int processTargets(Atom*,const char**,int) { return FALSE; };
    virtual int isFileTarget(Atom) { return FALSE; };
    virtual int processFileTargetWrapper(Atom,XtPointer,unsigned int) { return FALSE; };
    
    // internal processing
    Atom acceptDropWrapper(unsigned int,Widget);
    
    // internal callback stubs
    static void DragProc(Widget,XtPointer,XtPointer);
    static void DropProc(Widget,XtPointer,XtPointer);
    static void DestroyCb(Widget,XtPointer,XtPointer);
    static void TransferProc(Widget,XtPointer,Atom*,Atom*,XtPointer,unsigned long*,int*);

    // internal callback methods
    void dragProc(XmDragProcCallbackStruct*);
    void dropProc(XmDropProcCallbackStruct*);
    void handleDrop(XmDropProcCallbackStruct*);
    void transferProc(Widget,Atom,XtPointer,unsigned int);
private:
};


//
// XFE_DropDesktop
//

class XFE_DropDesktop : public XFE_DropBase
{
public:
    XFE_DropDesktop(Widget);
    virtual ~XFE_DropDesktop();
protected:
    // base class methods changed
    int isFileTarget(Atom);
    int processFileTargetWrapper(Atom,XtPointer,unsigned int);
    
    // drop processing
    void acceptFileTargets();
    int processSGI_URL(char*,unsigned int);
    int processSGI_ICON(char*,unsigned int);
    int processFILE_NAME(char*,unsigned int);

    // derived class can override to get file drops
    virtual int processDesktopURLTarget(const char*,const char*) { return FALSE; };
    virtual int processFileTarget(const char**,int) { return FALSE; };
private:
};

//
// XFE_DropNetscape
//

class XFE_DropNetscape : public XFE_DropDesktop 
{
public:
    XFE_DropNetscape(Widget);
    virtual ~XFE_DropNetscape();
protected:
    int processFileTarget(const char**,int);
    int processDesktopURLTarget(const char*,const char*);

    // derived class override's processTargets() to receive drop data
private:
};


#endif // _DRAG_DROP_H
