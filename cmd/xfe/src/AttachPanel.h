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
   AttachPanel.h -- class definitions for attachment panel and icon
   Created: Alastair Gourlay(SGI) c/o Dora Hsu<dora@netscape.com>, 26 Nov 1996
 */



#ifndef _ATTACH_PANEL_H
#define _ATTACH_PANEL_H

// Classes in this file:
//      XFE_AttachPanel
//      XFE_AttachPanelItem
//

#include "MNView.h"
#include "Component.h"
#include <Xm/Xm.h>

class XFE_AttachPanelItem;

//
// XFE_AttachPanel - attachment window
//

class XFE_AttachPanel : public XFE_Component {
public:
    XFE_AttachPanel(MWContext*);
    virtual ~XFE_AttachPanel();

    // widget interfaces
    virtual void createWidgets(Widget);
    operator Widget() { return getBaseWidget(); };
    Widget pane() { return _pane; };
    void armedWidget(Widget w) { _armedWidget=w; };
    MWContext *context() { return _context; };
    virtual void mapPane();
    virtual void unmapPane();
    virtual void doubleClickCb(int);
    void multiClickEnabled(int v) { _multiClickEnabled=v; };
    int multiClickEnabled() { return _multiClickEnabled; };
    XtTranslations iconTranslations() { return _iconTranslations; };
    int isAttachPanelWidget(Widget);

    // item list management
    XFE_AttachPanelItem **items();
    int numItems();
    void addItem(XFE_AttachPanelItem*,int pos=-1);
    void addItem(const char *,const char *itemLabel=NULL,const char *itemType=NULL,int pos=-1);
    void removeItem(XFE_AttachPanelItem*);
    void removeItem(int);
    void removeAllItems();
    int findItem(XFE_AttachPanelItem*);
    XFE_AttachPanelItem* currentSelection() { return _currentSelection; };
    int currentSelectionPos() { return findItem(_currentSelection); };
    void selectItem(XFE_AttachPanelItem*);
    void deselectItem(XFE_AttachPanelItem*);
    void traverseItem(int);
    void scrollToItem(int);

    // display management
    void updateDisplay();
    int getPreferredHeight();
    
protected:
    MWContext* _context;
    // Motif information
    Widget _parent;
    Widget _top;
    Widget _topClip;
    Widget _topScrollBar;
    Widget _pane;
    Widget _popupMenu;
    Widget _popupParent;
    XtIntervalId _resizeProcId;
    XtIntervalId _traversalProcId;
    int _inDestructor;
    int _firstExpose;
    Time _popupLastEventTime;
    Widget _armedWidget;
    Widget _traversalWidget;
    int _multiClickEnabled;
    XtTranslations _iconTranslations;
    int _prefHeight;

    // panel contents    
    XFE_AttachPanelItem **_items;
    int _numItems;
    int _allocItems;
    XFE_AttachPanelItem *_currentSelection;

    // per-class settings
    static const int _allocIncrement;
    static const int _marginWidth;
    static const int _marginHeight;
    static const int _horizSpacing;
    
    // internal display management
    void layout();
    virtual void realize();
    virtual void updateSelectionUI() { };    

    // internal popup menu management
    void initializePopupMenu();
    
    // internal callback stubs
    static void ExposeCb(Widget,XtPointer,XtPointer);
    static void InputCb(Widget,XtPointer,XtPointer);
    static void ScrollTraversalCb(Widget,XtPointer,XtPointer);
    static void ScrollTraversalProc(XtPointer,XtIntervalId*);
    static void ScrollResizeCb(Widget,XtPointer,XtPointer);
    static void ScrollResizeProc(XtPointer,XtIntervalId*);
    static void PopupProc(Widget,XtPointer,XEvent*,Boolean*);
    static void PopdownCb(Widget,XtPointer,XtPointer);

    // internal callback methods
    void inputCb(XEvent*);
    void scrollTraversalProc(Widget);
    void scrollResizeProc();
private:
};


//
// XFE_AttachPanelItem
//

class XFE_AttachPanelItem
{
public:
    XFE_AttachPanelItem(XFE_AttachPanel*,const char *data=NULL,const char *dataLabel=NULL,const char *dataType=NULL);
    ~XFE_AttachPanelItem();
    const char *dataLabel();
    const char *dataType();
    const char *data();
    void calculatePrefGeometry();
    Widget label() { return _label; };
    Widget image() { return _image; };
    int prefWidth() { return _prefWidth; };
    int prefHeight() { return _prefHeight; };
    int imageWidth() { return _imageWidth; };
    int imageHeight() { return _imageHeight; };
    int labelWidth() { return _labelWidth; };
    int labelHeight() { return _labelHeight; };
    void layout(int,int,unsigned int,unsigned int);
    void realize();
    void show();
    void hide();
    void traverse();
    void scrollVisible();
    void select(int selected=TRUE);
    void inParentDestructor();
    int beingDestroyed() { return _beingDestroyed; };
protected:
    XFE_AttachPanel *_attachPanel;
    int _inParentDestructor;
    int _beingDestroyed;
    char *_data;
    char *_dataLabel;
    char *_dataType;
    unsigned int _prefWidth;
    unsigned int _prefHeight;
    unsigned int _imageWidth;
    unsigned int _imageHeight;
    unsigned int _labelWidth;
    unsigned int _labelHeight;
    
    static Pixmap _audioPixmap;
    static Pixmap _binaryPixmap;
    static Pixmap _imagePixmap;
    static Pixmap _messagePixmap;
    static Pixmap _moviePixmap;
    static Pixmap _textPixmap;
    static Pixmap _unknownPixmap;
    static Pixmap _urlPixmap;
    static Pixmap _vcardPixmap;
    static Cursor _cursor;

    Widget _parent;
    Widget _image;
    Widget _label;

    // internal methods
    Pixmap iconPixmap();
    
    // internal callback stubs
    static void ActivateCb(Widget,XtPointer,XtPointer);
    static void ArmCb(Widget,XtPointer,XtPointer);
    static void DisarmCb(Widget,XtPointer,XtPointer);

    // internal callback methods
    void activateCb();
    void doubleClickCb();
    void armCb(Widget);
    void disarmCb();
private:
};


#endif // _ATTACH_PANEL_H
