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
   AttachPanel.cpp -- panel for displaying icon view of attached files.
   Created: Alastair Gourlay(SGI) c/o Dora Hsu<dora@netscape.com>, 26 Nov 1996
 */



// Classes in this file:
//      XFE_AttachPanel
//      XFE_AttachPanelItem
//


#include "AttachPanel.h"
#include <stdlib.h>
#include <Xm/XmAll.h>
#include <X11/cursorfont.h>
#include "mozilla.h"
#include "xfe.h"
#include "net.h"
#include "icons.h"
#include "icondata.h"
#include "DragDrop.h"

#ifdef DEBUG_sgidev
#define XDEBUG(x) x
#else
#define XDEBUG(x)
#endif

//
// XFE_AttachPanel
//

// static initialization

const int XFE_AttachPanel::_allocIncrement=50; // item list management
const int XFE_AttachPanel::_marginWidth=5; // panel margin
const int XFE_AttachPanel::_marginHeight=5; // panel margin
const int XFE_AttachPanel::_horizSpacing=10; // panel spacing

#define ATTACH_ICON_NAME "attachItemImage"
#define ATTACH_LABEL_NAME "attachItemLabel"

// callback stubs

void XFE_AttachPanel::ExposeCb(Widget,XtPointer cd,XtPointer) {
    XFE_AttachPanel *ad=(XFE_AttachPanel*)cd;
    if (ad && ad->_firstExpose) {
	ad->_firstExpose=FALSE;
        ad->realize();
    }
}

void XFE_AttachPanel::InputCb(Widget,XtPointer cd,XtPointer cb) {
    XFE_AttachPanel *ad=(XFE_AttachPanel*)cd;
    
    if (ad && cb) {
        XmDrawingAreaCallbackStruct *dcb=(XmDrawingAreaCallbackStruct*)cb;
        ad->inputCb(dcb->event);
    }
}

void XFE_AttachPanel::ScrollTraversalCb(Widget,XtPointer cd,XtPointer cb) {
    XFE_AttachPanel *ad=(XFE_AttachPanel*)cd;
    XmTraverseObscuredCallbackStruct *scb=(XmTraverseObscuredCallbackStruct*)cb;
    if (ad && ad->_traversalProcId==0 &&
        scb && scb->reason==XmCR_OBSCURED_TRAVERSAL) {
        // delay scroll until after completion of armCb(). Of course :)
        // if only Motif passed a valid cb->event, all this would be unecessary
        ad->_traversalWidget=scb->traversal_destination;
        ad->_traversalProcId=XtAppAddTimeOut(XtWidgetToApplicationContext(ad->_top),
                                   0,ScrollTraversalProc,cd);
    }
}

void XFE_AttachPanel::ScrollTraversalProc(XtPointer cd, XtIntervalId *id) {
    XFE_AttachPanel *ad=(XFE_AttachPanel*)cd;
    if (ad) {
        if (ad->_traversalProcId==*id)
            ad->_traversalProcId=0;
        if (ad->_traversalWidget) {
            ad->scrollTraversalProc(ad->_traversalWidget);
            ad->_traversalWidget=NULL;
        }
    }
}

void XFE_AttachPanel::ScrollResizeCb(Widget,XtPointer cd,XtPointer) {
    XFE_AttachPanel *ad=(XFE_AttachPanel*)cd;
    if (ad && ad->_resizeProcId==0) {
        // delay resize until after completion of resize callback. Of course :)
        ad->_resizeProcId=XtAppAddTimeOut(XtWidgetToApplicationContext(ad->_top),
                                   0,ScrollResizeProc,cd);
    }
}
void XFE_AttachPanel::ScrollResizeProc(XtPointer cd, XtIntervalId *id) {
    XFE_AttachPanel *ad=(XFE_AttachPanel*)cd;
    if (ad) {
        if (ad->_resizeProcId==*id)
            ad->_resizeProcId=0;
        ad->scrollResizeProc();
    }
}


void XFE_AttachPanel::PopupProc(Widget,XtPointer cd,XEvent *event,Boolean*)
{
    XFE_AttachPanel *ad=(XFE_AttachPanel*)cd;

    if (!ad->_popupMenu) return;
    if (event->xany.type!=ButtonPress) return;
    if (((XButtonEvent *)event)->button != Button3) return;
    if (((XButtonEvent *)event)->time <= ad->_popupLastEventTime) return;
    XmMenuPosition(ad->_popupMenu,(XButtonEvent*)event);
    XtManageChild (ad->_popupMenu);
}

/* By default, cancelling a popup menu with Button 3 will cause the
 * popup to be reposted at the location of the cancelling click.
 *
 * To switch off this behavior, remember when the menu was popped down.
 * In PopupHandler, don't repost the menu if the posting click just
 * cancelled a popup menu.
 */
void XFE_AttachPanel::PopdownCb(Widget w,XtPointer cd,XtPointer)
{
    XFE_AttachPanel *ad=(XFE_AttachPanel*)cd;

    ad->_popupLastEventTime = XtLastTimestampProcessed (XtDisplay(w));
}



// constructor

XFE_AttachPanel::XFE_AttachPanel(MWContext* context) : XFE_Component()
{
    _context=context;
    
    _inDestructor=FALSE;
    _parent=NULL;
    _top=NULL;
    _topClip=NULL;
    _topScrollBar=NULL;
    _pane=NULL;
    _resizeProcId=0;
    _traversalProcId=0;
    _firstExpose=TRUE;
    _popupParent=NULL;
    _popupMenu=NULL;
    _popupLastEventTime=0;
    _armedWidget=NULL;
    _traversalWidget=NULL;
    _multiClickEnabled=FALSE;
    _iconTranslations=NULL;
    _prefHeight=0;
    
    _items=NULL;
    _numItems=0;
    _allocItems=0;
    _currentSelection=NULL;    
}

XFE_AttachPanel::~XFE_AttachPanel()
{
    _inDestructor=TRUE;

    if (_resizeProcId!=0)
        XtRemoveTimeOut(_resizeProcId);

    if (_traversalProcId!=0)
        XtRemoveTimeOut(_traversalProcId);

    // XFE_Component will destroy the widget tree
}

void XFE_AttachPanel::createWidgets(Widget parent)
{
    _parent=parent;
    
    Arg args[10];
    
    // toplevel scrolled window
    
    XtSetArg(args[0],XmNscrollingPolicy, XmAUTOMATIC);
    _top=XmCreateScrolledWindow(_parent,"attach",args,1);
    XtAddCallback(_top,XmNtraverseObscuredCallback,ScrollTraversalCb,(XtPointer)this);
    //XtManageChild(_top);
    Widget vsb;
    XtVaGetValues(_top,XmNverticalScrollBar,&vsb,NULL);
    if (vsb) XtVaSetValues(vsb,XmNtraversalOn,False,NULL);

    // attachment pane
    _pane=XmCreateDrawingArea(_top,"pane",NULL,0);
    XtVaSetValues(_pane,
                  XmNmarginWidth,0,
                  XmNmarginHeight,0,
                  NULL);
    // nyi - best way to avoid multiple redraw?
    XtAddCallback(_pane,XmNexposeCallback,ExposeCb,(XtPointer)this);
    XtAddCallback(_pane,XmNinputCallback,InputCb,(XtPointer)this);
    XtManageChild(_pane);

    // only want vertical scrolling - make form track width of outer window
    Widget hsb;
    XtVaGetValues(_top,
                  XmNclipWindow,&_topClip,
                  XmNverticalScrollBar,&_topScrollBar,
                  XmNhorizontalScrollBar,&hsb,
                  NULL);
    if (XmIsDrawingArea(_topClip)) { // it better be!
        XtAddCallback(_topClip,XmNresizeCallback,ScrollResizeCb,(XtPointer)this);
        XtAddCallback(_topClip,XmNinputCallback,InputCb,(XtPointer)this);
    }
    
    // hide horizontal scrollbar - prevent it flickering on during resize
    if (hsb) {
        XtVaSetValues(hsb,
                      XmNheight,1,
                      XmNmappedWhenManaged,FALSE,
                      NULL);
    }
    
    // make scroller background match child
    Pixel bg;
    XtVaGetValues(_pane,XmNbackground,&bg,NULL);
    XtVaSetValues(_topClip,XmNbackground,bg,NULL);
    
    _popupParent=_top;

    setBaseWidget(_top);
}

void XFE_AttachPanel::initializePopupMenu()
{
    if (!_popupMenu || !_popupParent)
        return;

    XtAddEventHandler (_popupParent,ButtonPressMask,False,PopupProc,this);
    //XtAddCallback(XtParent(_popupMenu),XmNpopdownCallback,PopdownCb,this);

    updateSelectionUI();
}

//
// item list management
//

void XFE_AttachPanel::addItem(const char *itemData,const char *itemLabel,const char *itemType,int pos)
{
    if (!itemData || strlen(itemData)==0)
        return;

    XFE_AttachPanelItem *ai=new XFE_AttachPanelItem(this,itemData,itemLabel,itemType);
    addItem(ai,pos);
}


void XFE_AttachPanel::addItem(XFE_AttachPanelItem *newItem,int pos) 
{
    int i;

    if (pos==-1)
        pos=_numItems;

    if (pos<0 || pos>_numItems)
        return;

    // save pointer to passed item.
    // adopt it, and delete in removeItem(), destructor
    if (_numItems >= _allocItems) {
        _allocItems+=_allocIncrement;
        XFE_AttachPanelItem **newList=new XFE_AttachPanelItem*[_allocItems];
        for (i=0;i<_numItems;i++) {
            newList[i]=_items[i];
            _items[i]=NULL;
        }
        if (_items)
            delete _items;
        _items=newList;
    }

    for (i=_numItems;i>pos;i--) {
        _items[i]=_items[i-1];
    }
    _items[pos]=newItem;
    _numItems++;
}

int XFE_AttachPanel::findItem(XFE_AttachPanelItem *ai)
{
    if (ai==NULL)
        return -1;
    
    for (int i=0;i<_numItems;i++) {
        if (_items[i]==ai) {
            return i;
        }
    }
    return -1;
}


void XFE_AttachPanel::removeItem(XFE_AttachPanelItem *item)
{
    int pos=findItem(item);

    if (pos>=0)
        removeItem(pos);
}


void XFE_AttachPanel::removeItem(int r)
{
    // Warning: never decreases list alloc'd size
    // if XFE_AttachPanel object is long-lived, then it should.
    // use removeAllItems() method to force a cleanup.

    if (r<0 || r>=_numItems)
        return;

    if (_items[r]) {
        // if focus is on last item, move it to last-but-one item
        // to ensure that attach panel keeps focus after delete.
        if (r==_numItems-1 && r>0) {
            Widget focusWidget=XmGetFocusWidget(getBaseWidget());
            Widget lastButOne=NULL;
            if (_items[r-1])
                lastButOne=_items[r-1]->image();
            if (focusWidget &&
                focusWidget==_items[r]->image() &&
                lastButOne!=NULL) {    
                XmProcessTraversal(lastButOne,XmTRAVERSE_CURRENT);
            }    
        }
            
        delete _items[r];
    }
    
    for (int i=r; i<_numItems-1;i++)
        _items[i]=_items[i+1];
    _items[_numItems-1]=NULL;
    _numItems--;
}


void XFE_AttachPanel::removeAllItems()
{
    if (_items) {
        for (int i=0; i<_numItems;i++) {
            if (_items[i]) {
                if (_inDestructor)
                    _items[i]->inParentDestructor();
                delete _items[i];
                _items[i]=NULL;
            }
        }
        delete _items;
        _items=NULL;
        _numItems=0;
        _allocItems=0;
    }
}


XFE_AttachPanelItem **XFE_AttachPanel::items()
{
    return _items;
}


int XFE_AttachPanel::numItems()
{
    return _numItems;
}

void XFE_AttachPanel::selectItem(XFE_AttachPanelItem* item)
{
    if (!item || item->beingDestroyed())
        return;

    if (_currentSelection && item!=_currentSelection)
        _currentSelection->select(FALSE);
    
    item->select(TRUE);
    _currentSelection=item;

    updateSelectionUI();
}

void XFE_AttachPanel::deselectItem(XFE_AttachPanelItem* item)
{
    if (!item || item!=_currentSelection)
        return;

    if (!item->beingDestroyed())
        item->select(FALSE);
    
    _currentSelection=NULL;

    updateSelectionUI();
}

//
// display management
//

void XFE_AttachPanel::updateDisplay()
{
        layout();
}

void XFE_AttachPanel::traverseItem(int pos)
{
    if (pos<0 || pos >=_numItems)
        return;

    _items[pos]->traverse();
}

void XFE_AttachPanel::scrollToItem(int pos)
{
    if (pos<0 || pos >=_numItems)
        return;

    _items[pos]->scrollVisible();
}

int XFE_AttachPanel::getPreferredHeight()
{
    if (_prefHeight>0)
        return _prefHeight;
    else
        return 20;
}

//#define DEBUG_LAYOUT
void XFE_AttachPanel::layout()
{
#if 0
    if (_firstExpose)
        return;
#endif
    
    if (_items==NULL || _numItems==0)
        return;

    // examine items
    int i;
    int maxWidth=0;
    int maxHeight=0;
    for (i=0;i<_numItems;i++) {        
        if (_items[i] && !_items[i]->beingDestroyed()) {
            _items[i]->calculatePrefGeometry();
            if (_items[i]->prefWidth()>maxWidth) maxWidth=_items[i]->prefWidth();
            if (_items[i]->prefHeight()>maxHeight) maxHeight=_items[i]->prefHeight();
            
        }
    }

    if (maxWidth==0 || maxHeight==0) 
        return;

    // calculate basic parameters
    Position wd;
    XtVaGetValues(_topClip,XmNwidth,&wd,NULL);
    unsigned int availWidth=(unsigned int)(wd>2*_marginWidth ?
                                           (wd-2*_marginWidth) : 1);
    unsigned int vSpacing=(_items[0] ? _items[0]->labelHeight()/2 : 10);
    //nyi - need a good way to calc cell width - deal with large, rare cases
    unsigned int cellWidth=maxWidth+_horizSpacing;
    if (cellWidth<1) cellWidth=1;
    unsigned int cellHeight=maxHeight;
    int cellsPerRow=availWidth/cellWidth;
    if (cellsPerRow<1) cellsPerRow=1;
    
    // lay out row by row.
    // give each item enough cells to prevent overlap
#ifdef DEBUG_LAYOUT
    printf("\nlayout()\n\n");
    printf("  availWidth=%d\n",availWidth);
    printf("  cellWidth=%d\n",cellWidth);
    printf("  cellHeight=%d\n",cellHeight);
    printf("  cellsPerRow=%d\n",cellsPerRow);
    printf("\n");
#endif
    
    int itemPos=0;
    int cellNum=0;
    int row=0;

    // if an icon has focus, restore it when we remap the pane
    Widget focusWidget=XmGetFocusWidget(_top);
    if (focusWidget && XtParent(focusWidget)!=_pane)
        focusWidget=NULL;
    
    unmapPane();

    // calculate ideal height for this layout + border
    Dimension st;
    XtVaGetValues(_top,XmNshadowThickness,&st,NULL);
    _prefHeight=2*st + 2*_marginHeight + cellHeight + vSpacing;

    // do layout
    while (itemPos<_numItems) {
#ifdef DEBUG_LAYOUT        
        printf("[%d]\n",itemPos);
#endif        
        if (_items[itemPos] && !_items[itemPos]->beingDestroyed()) {
            int numCells=_items[itemPos]->prefWidth()/cellWidth + 1;
            if (numCells>cellsPerRow) numCells=cellsPerRow;
#ifdef DEBUG_LAYOUT            
            printf("  numCells=%d\n",numCells);
#endif            
            // see if it makes sense to move to a new row
            if (numCells>cellsPerRow-cellNum && cellNum!=0) {
                row++;
                _prefHeight+=cellHeight+vSpacing;
                cellNum=0;
            }
#ifdef DEBUG_LAYOUT
            printf("  row:%d-%d    (%d,%d)  %dx%d\n",
                   row,cellNum,
                   cellNum*cellWidth,row*cellHeight,
                   numCells*cellWidth,cellHeight);
#endif                   
            
            //do layout
            _items[itemPos]->layout(_marginWidth+cellNum*cellWidth,
                                    _marginHeight+row*(cellHeight+vSpacing),
                                    numCells*cellWidth,
                                    cellHeight);
            cellNum+=numCells;
        }
        itemPos++;
    }

    mapPane();
    
    // restore focus if we had it before the unmapPane()
    if (focusWidget)
        XmProcessTraversal(focusWidget,XmTRAVERSE_CURRENT);
}

void XFE_AttachPanel::mapPane() 
{
    if (_pane)
        XtSetMappedWhenManaged(_pane,TRUE);
    if (_topScrollBar)
        XtSetMappedWhenManaged(_topScrollBar,TRUE);
}

void XFE_AttachPanel::unmapPane() 
{
    if (_pane)
        XtSetMappedWhenManaged(_pane,FALSE);
    if (_topScrollBar)
        XtSetMappedWhenManaged(_topScrollBar,FALSE);
}

int XFE_AttachPanel::isAttachPanelWidget(Widget w)
{
    if (w==NULL)
        return FALSE;
    
    if (w==_top ||
        w==_topClip ||
        w==_topScrollBar ||
        w==_pane ||
        XtParent(w)==_pane)
        return TRUE;
    
    return FALSE;
}


//
// callback methods
//

void XFE_AttachPanel::realize()
{
    // realize processing for existing items
    if (_items) {
        for (int i=0;i<_numItems;i++)
            _items[i]->realize();
        layout();
    }
}

void XFE_AttachPanel::doubleClickCb(int)
{
    // no default double-click action - it's for the derived classes.
}

void XFE_AttachPanel::inputCb(XEvent *event)
{
    // button up on panel background cancels selection
    if (_currentSelection!=NULL &&
        event &&
        event->xany.type==ButtonRelease &&
        event->xbutton.button==1) {
        deselectItem(_currentSelection);
    }
}


void XFE_AttachPanel::scrollTraversalProc(Widget dest)
{
    // let disarmCb handle scrolling for button click
    if (!dest || dest==_armedWidget)
        return;

    char *destName=XtName(dest);    
    if (!destName || strcmp(destName,ATTACH_ICON_NAME)!=0)
        return;

    // if this is the icon of XFE_AttachItem, scroll it into view.
    XtPointer userData;
    XtVaGetValues(dest,XmNuserData,&userData,NULL);
    if (!userData)
        return;

    XFE_AttachPanelItem *ai=(XFE_AttachPanelItem*)userData;

    ai->scrollVisible();
}

void XFE_AttachPanelItem::scrollVisible() 
{
    // scroll to icon, with enough margin to show label

    XmScrollVisible(_attachPanel->getBaseWidget(),_image,0,labelHeight()+2);
    XmScrollVisible(_attachPanel->getBaseWidget(),_label,0,0);
}

void XFE_AttachPanel::scrollResizeProc()
{
    const int rspace=10; // must be >= 2, or window will oscillate
    Dimension width;

    if (!_topClip || !_pane)
	return;

    XtVaGetValues(_topClip,XmNwidth,&width,NULL);
    if (width<rspace+1)
        width=rspace+1;
    XtVaSetValues(_pane,XmNwidth,width-rspace,NULL);

    layout();
}

//
// XFE_AttachPanelItem
//

// static initialization

Pixmap XFE_AttachPanelItem::_audioPixmap=XmUNSPECIFIED_PIXMAP;
Pixmap XFE_AttachPanelItem::_binaryPixmap=XmUNSPECIFIED_PIXMAP;
Pixmap XFE_AttachPanelItem::_imagePixmap=XmUNSPECIFIED_PIXMAP;
Pixmap XFE_AttachPanelItem::_messagePixmap=XmUNSPECIFIED_PIXMAP;
Pixmap XFE_AttachPanelItem::_moviePixmap=XmUNSPECIFIED_PIXMAP;
Pixmap XFE_AttachPanelItem::_textPixmap=XmUNSPECIFIED_PIXMAP;
Pixmap XFE_AttachPanelItem::_unknownPixmap=XmUNSPECIFIED_PIXMAP;
Pixmap XFE_AttachPanelItem::_vcardPixmap=XmUNSPECIFIED_PIXMAP;
Pixmap XFE_AttachPanelItem::_urlPixmap=XmUNSPECIFIED_PIXMAP;
Cursor XFE_AttachPanelItem::_cursor=None;


// callback stubs

void XFE_AttachPanelItem::ActivateCb(Widget,XtPointer cd,XtPointer cb) {
    XFE_AttachPanelItem *ad=(XFE_AttachPanelItem*)cd;
    XmPushButtonCallbackStruct *pcb=(XmPushButtonCallbackStruct*)cb;
    
    if (ad && pcb) {
        if (ad->_attachPanel->multiClickEnabled()) {
            switch(pcb->click_count) {
            case 1: ad->activateCb(); break;
            case 2: ad->doubleClickCb(); break;
            default: break;
            }
        }
        else {
            ad->activateCb();
        }
    }
}

void XFE_AttachPanelItem::ArmCb(Widget w,XtPointer cd,XtPointer) {
    XFE_AttachPanelItem *ad=(XFE_AttachPanelItem*)cd;
    if (ad)
        ad->armCb(w);
}

void XFE_AttachPanelItem::DisarmCb(Widget,XtPointer cd,XtPointer) {
    XFE_AttachPanelItem *ad=(XFE_AttachPanelItem*)cd;
    if (ad)
        ad->disarmCb();
}


// constructor

XFE_AttachPanelItem::XFE_AttachPanelItem(XFE_AttachPanel *attach,
                                         const char *data,
                                         const char *dataLabel,
                                         const char *dataType)
{
    _attachPanel=attach;
    
    _inParentDestructor=FALSE;
    _beingDestroyed=FALSE;
    
    if (data)
        _data=strdup(data);
    else
        _data=NULL;

    if (dataLabel)
        _dataLabel=strdup(dataLabel);
    else
        _dataLabel=(_data ? strdup(_data) : (char*)NULL);

    if (dataType) {
        // app specified a mime type for this data
        _dataType=strdup(dataType);
    }
    else {
        // try to determine type of attachment
        _dataType=strdup(XFE_DragBase::guessUrlMimeType(data));
    }

    _parent=_attachPanel->pane();

    _image=NULL;
    _label=NULL;
    _prefWidth=1;
    _prefHeight=1;

    // use link cursor over attach icons to indicate that they are active
    if (_cursor==None)
        _cursor=CONTEXT_DATA(_attachPanel->context())->link_cursor;
    
    // create widgets
    
    Arg args[10];

    Pixel bg;
    XtVaGetValues(_parent,XmNbackground,&bg,NULL);

    // image
    
    XtSetArg(args[0],XmNbackground,bg);
    _image=XmCreatePushButton(_parent,ATTACH_ICON_NAME,args,1);
    XtVaSetValues(_image,
                  XmNalignment,XmALIGNMENT_CENTER,
                  XmNshadowThickness,0,
                  XmNmarginWidth,0,
                  XmNmarginHeight,0,
                  XmNlabelType,XmPIXMAP,
                  XmNlabelPixmap,iconPixmap(),
                  XmNarmColor,bg,
                  XmNmultiClick,XmMULTICLICK_KEEP,
                  XmNuserData,(XtPointer)this, // scrollTraversalProc needs to get to item class
                  NULL);
    XtAddCallback(_image,XmNactivateCallback,ActivateCb,(XtPointer)this);
    XtAddCallback(_image,XmNarmCallback,ArmCb,(XtPointer)this);
    XtAddCallback(_image,XmNdisarmCallback,DisarmCb,(XtPointer)this);
    if (_attachPanel->iconTranslations())
        XtOverrideTranslations(_image,_attachPanel->iconTranslations());

    // label
    
    XtSetArg(args[0],XmNbackground,bg);
    _label=XmCreateLabel(_parent,ATTACH_LABEL_NAME,args,1);
    XmString xms=XmStringCreateLocalized(_dataLabel ? _dataLabel : "");
    XtVaSetValues(_label,
                  XmNalignment,XmALIGNMENT_CENTER,
                  XmNshadowThickness,0,
                  //XmNmarginWidth,0,
                  //XmNmarginHeight,0,
                  XmNlabelString,xms,
                  XmNuserData,FALSE, // selection marker for AttachPanelItem::select()
                  NULL);
    XmStringFree(xms);

    show();
    calculatePrefGeometry();
    if (XtWindow(_image)) {
        XDefineCursor(XtDisplay(_image),XtWindow(_image),_cursor);
    }
}

// destructor

XFE_AttachPanelItem::~XFE_AttachPanelItem()
{
    _beingDestroyed=TRUE;

    if (this==_attachPanel->currentSelection())
        _attachPanel->deselectItem(this);
    
    if (_data)
        free((void*)_data);

    if (_dataLabel)
        free((void*)_dataLabel);

    if (_dataType)
        free((void*)_dataType);

    if (_image) {
        XtUnmanageChild(_image);
        if (!_inParentDestructor)
            XtDestroyWidget(_image);
    }
    
    if (_label) {
        XtUnmanageChild(_label);
        if (!_inParentDestructor)
            XtDestroyWidget(_label);
    }
}

void XFE_AttachPanelItem::inParentDestructor()
{
    _inParentDestructor=TRUE;
}

void XFE_AttachPanelItem::realize()
{
    if (_image) {
        XtVaSetValues(_image,XmNlabelPixmap,iconPixmap(),NULL);
        calculatePrefGeometry();
        XDefineCursor(XtDisplay(_image),XtWindow(_image),_cursor);
    }
    
    if (this==_attachPanel->currentSelection())
        select(TRUE);
    else
        select(FALSE);
}


// this method should only be called when creating item.

Pixmap XFE_AttachPanelItem::iconPixmap()
{
    if (!XtIsRealized(_parent))
        return XmUNSPECIFIED_PIXMAP;

    XDEBUG(printf("iconPixmap(%s)\n",_dataType?_dataType:"NULL"));
    
    // return pixmap matching item's mime type
    // create Pixmaps on first request

    MWContext *context=_attachPanel->context();
    
    Pixel bg;
    XtVaGetValues(_parent,XmNbackground,&bg,NULL);
    
    if (!_dataType ||
        strcmp(_dataType,UNKNOWN_CONTENT_TYPE)==0) {
        if (_unknownPixmap==XmUNSPECIFIED_PIXMAP) {
            fe_icon icon={ 0 };
            fe_MakeIcon(context,bg, &icon, NULL,
                        GUnknown.width, GUnknown.height,
                        GUnknown.mono_bits, GUnknown.color_bits, GUnknown.mask_bits,
                        FALSE);
            _unknownPixmap=icon.pixmap;
        }
        return _unknownPixmap;
    }

    // internal type name - used for attached document URL's
    if (strncmp(_dataType,"_url",5)==0){
        if (_urlPixmap==XmUNSPECIFIED_PIXMAP) {
            fe_icon icon={ 0 };
            fe_MakeIcon(context,bg, &icon, NULL,
                        LocationProxy.width, LocationProxy.height,
                        LocationProxy.mono_bits, LocationProxy.color_bits, LocationProxy.mask_bits,
                        FALSE);
            _urlPixmap=icon.pixmap;
        }
        return _urlPixmap;
    }
    
    if (strncmp(_dataType,"audio",5)==0){
        if (_audioPixmap==XmUNSPECIFIED_PIXMAP) {
            fe_icon icon={ 0 };
            fe_MakeIcon(context,bg, &icon, NULL,
                        GAudio.width, GAudio.height,
                        GAudio.mono_bits, GAudio.color_bits, GAudio.mask_bits,
                        FALSE);
            _audioPixmap=icon.pixmap;
        }
        return _audioPixmap;
    }
    
    if (strncmp(_dataType,"application",11)==0) {
        if (_binaryPixmap==XmUNSPECIFIED_PIXMAP) {
            fe_icon icon={ 0 };
            fe_MakeIcon(context,bg, &icon, NULL,
                        GBinary.width, GBinary.height,
                        GBinary.mono_bits, GBinary.color_bits, GBinary.mask_bits,
                        FALSE);
            _binaryPixmap=icon.pixmap;
        }
        return _binaryPixmap;
    }
    
    if (strncmp(_dataType,"image",5)==0) {
        if (_imagePixmap==XmUNSPECIFIED_PIXMAP) {
            fe_icon icon={ 0 };
            fe_MakeIcon(context,bg, &icon, NULL,
                        GImage.width, GImage.height,
                        GImage.mono_bits, GImage.color_bits, GImage.mask_bits,
                        FALSE);
            _imagePixmap=icon.pixmap;
        }
        return _imagePixmap;
    }
    
    if (strncmp(_dataType,"message",7)==0) {
        if (_messagePixmap==XmUNSPECIFIED_PIXMAP) {
            fe_icon icon={ 0 };
            fe_MakeIcon(context,bg, &icon, NULL,
                        MNTB_Forward.width, MNTB_Forward.height,
                        MNTB_Forward.mono_bits, MNTB_Forward.color_bits, MNTB_Forward.mask_bits,
                        FALSE);
            _messagePixmap=icon.pixmap;
        }        
        return _messagePixmap;
    }
    
    if (strncmp(_dataType,"video",5)==0) {
        if (_moviePixmap==XmUNSPECIFIED_PIXMAP) {
            fe_icon icon={ 0 };
            fe_MakeIcon(context,bg, &icon, NULL,
                        GMovie.width, GMovie.height,
                        GMovie.mono_bits, GMovie.color_bits, GMovie.mask_bits,
                        FALSE);
            _moviePixmap=icon.pixmap;
        }        
        return _moviePixmap;
    }
    
    if (strcmp(_dataType,"text/x-vcard")==0) {
        if (_vcardPixmap==XmUNSPECIFIED_PIXMAP) {
            fe_icon icon={ 0 };
            fe_MakeIcon(context,bg, &icon, NULL,
                        MNAB_NewPerson.width, MNAB_NewPerson.height,
                        MNAB_NewPerson.mono_bits, MNAB_NewPerson.color_bits, MNAB_NewPerson.mask_bits,
                        FALSE);
            _vcardPixmap=icon.pixmap;
        }
        return _vcardPixmap;
    }

    if (strncmp(_dataType,"text",4)==0) {
        if (_textPixmap==XmUNSPECIFIED_PIXMAP) {
            fe_icon icon={ 0 };
            fe_MakeIcon(context,bg, &icon, NULL,
                        GText.width, GText.height,
                        GText.mono_bits, GText.color_bits, GText.mask_bits,
                        FALSE);
            _textPixmap=icon.pixmap;
        }
        return _textPixmap;
    }

    // fallback to generic document pixmap
    
    if (_unknownPixmap==XmUNSPECIFIED_PIXMAP) {
        fe_icon icon={ 0 };
        fe_MakeIcon(context,bg, &icon, NULL,
                    GUnknown.width, GUnknown.height,
                    GUnknown.mono_bits, GUnknown.color_bits, GUnknown.mask_bits,
                    FALSE);
        _unknownPixmap=icon.pixmap;
    }
    return _unknownPixmap;
}


const char *XFE_AttachPanelItem::data() 
{
    return _data;
}

const char *XFE_AttachPanelItem::dataLabel() 
{
    return _dataLabel;
}

const char *XFE_AttachPanelItem::dataType() 
{
    return _dataType;
}

void XFE_AttachPanelItem::select(int selected)
{
    // Hack to do selection highlight.
    // May need to do real X drawing in AttachPanel[Item]::exposeCb()

    if (_label) {
        Pixel fg;
        Pixel bg;
        XtPointer userData;
        
        XtVaGetValues(_label,
                      XmNbackground,&bg,
                      XmNforeground,&fg,
                      XmNuserData,&userData,
                      NULL);

        int labelHighlighted=(int)userData;
        
        if (selected && !labelHighlighted) {
            XtVaSetValues(_label,
                          XmNbackground,fg,
                          XmNforeground,bg,
                          XmNuserData,1,
                          NULL);
        }

        if (!selected && labelHighlighted) {
            XtVaSetValues(_label,
                          XmNbackground,fg,
                          XmNforeground,bg,
                          XmNuserData,0,
                          NULL);
        }
    }
}

void XFE_AttachPanelItem::calculatePrefGeometry()
{
    Dimension iw,ih,lw,lh;
    
    XtVaGetValues(_image,
                  XmNwidth,&iw,
                  XmNheight,&ih,
                  NULL);
    XtVaGetValues(_label,
                  XmNwidth,&lw,
                  XmNheight,&lh,
                  NULL);

    _imageWidth=iw;
    _imageHeight=ih;
    _labelWidth=lw;
    _labelHeight=lh;

    // ? save image plus label geometry    
    _prefWidth=(unsigned int)(iw>lw ? iw : lw);
    _prefHeight=(unsigned int)(ih+lh);
}

void XFE_AttachPanelItem::layout(int x, int y, unsigned int width, unsigned int height) 
{
    if (_label==NULL || _image==NULL ||
        _prefWidth==0 || _prefHeight==0 ||
        width==0 || height==0 )
        return;
    
    int imageX;
    int imageY;
    int labelX;
    int labelY;    

    imageX=x+(width-_imageWidth)/2;
    imageY=y+height-_labelHeight-_imageHeight;
    labelX=x+(width-_labelWidth)/2;
    labelY=y+height-_labelHeight;

    XtVaSetValues(_image,
                  XmNx,imageX,
                  XmNy,imageY,
                  NULL);
    XtVaSetValues(_label,
                  XmNx,labelX,
                  XmNy,labelY,
                  NULL);
}


void XFE_AttachPanelItem::show() 
{
    if (_image)
        XtManageChild(_image);
    if (_label)
        XtManageChild(_label);
}

void XFE_AttachPanelItem::hide() 
{
    if (_image)
        XtUnmanageChild(_image);
    if (_label)
        XtUnmanageChild(_label);
}

void XFE_AttachPanelItem::traverse() 
{
    if (_image)
        XmProcessTraversal(_image,XmTRAVERSE_CURRENT);
}

//
// callback methods
//

void XFE_AttachPanelItem::activateCb()
{
    if (this!=_attachPanel->currentSelection())
        _attachPanel->selectItem(this);
    else
        _attachPanel->deselectItem(this);
}

void XFE_AttachPanelItem::doubleClickCb()
{
    _attachPanel->doubleClickCb(_attachPanel->findItem(this));
}

void XFE_AttachPanelItem::armCb(Widget w)
{
    // flag button as down, use to discard ScrollTraversalCb calls for button press
    // (since the event field in the traverseObscuredCallbackStruct is useless.)

    _attachPanel->armedWidget(w);
}

void XFE_AttachPanelItem::disarmCb()
{
    _attachPanel->armedWidget(NULL);
    
    // make sure label is visible after BtnUp on icon
    scrollVisible();
}


