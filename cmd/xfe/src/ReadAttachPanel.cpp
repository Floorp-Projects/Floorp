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
   ReadAttachPanel.cpp -- read window attachment panel
   Created: Alastair Gourlay(SGI) c/o Dora Hsu<dora@netscape.com>, 26 Nov 1996
 */

// Classes in this file:
//      XFE_ReadAttachPanel
//

#include "ReadAttachPanel.h"
#include <stdlib.h>
#include <Xm/XmAll.h>
#include "mozilla.h"
#include "xfe.h"
#include "net.h"
#include "icons.h"
#include "icondata.h"
#include "PopupMenu.h"

//
// XFE_ReadAttachPanel
//

// callback stubs

void XFE_ReadAttachPanel::OpenCb(Widget,XtPointer cd,XtPointer) {
    XFE_ReadAttachPanel *ad=(XFE_ReadAttachPanel*)cd;
    if (ad)
        ad->openCb();
}

void XFE_ReadAttachPanel::SaveCb(Widget,XtPointer cd,XtPointer) {
    XFE_ReadAttachPanel *ad=(XFE_ReadAttachPanel*)cd;
    if (ad)
        ad->saveCb();
}

void XFE_ReadAttachPanel::PropertiesCb(Widget,XtPointer cd,XtPointer) {
    XFE_ReadAttachPanel *ad=(XFE_ReadAttachPanel*)cd;
    if (ad)
        ad->propertiesCb();
}

// constructor

XFE_ReadAttachPanel::XFE_ReadAttachPanel(MWContext* context)
    : XFE_AttachPanel(context)
{
    _attachments=NULL;
    _pane=NULL;
    _propertiesDialog=NULL;
    _attachDrag=NULL;

    multiClickEnabled(TRUE);
}

// destructor

XFE_ReadAttachPanel::~XFE_ReadAttachPanel()
{
    // this may be causing double-free..  investigate MSG_Pane
    // removeAllAttachments();

    if (_attachDrag) {
        delete _attachDrag;
        _attachDrag=NULL;
    }
}

// create Motif UI

void XFE_ReadAttachPanel::createWidgets(Widget parent)
{
    // create panel UI

    XFE_AttachPanel::createWidgets(parent);

    // set up drag handler
    Widget dragWidget=XtNameToWidget(getBaseWidget(),"*pane");
    if (dragWidget)
        _attachDrag=new XFE_ReadAttachDrag(dragWidget,this);

    // create popup

    Arg args[10];
    int n=0;
    // disable keyboard accelerators and naviagtion in popup - they cause
    // crashes. In future, use XFE_PopupMenu when the menu items become real
    // xfeCmd's - it is more robust.
    XtSetArg(args[n],XmNtraversalOn,False);n++;
    _popupMenu = XFE_PopupMenu::CreatePopupMenu(_popupParent,
												"msgViewAttachPopup",args,n);

    _open=XmCreatePushButtonGadget(_popupMenu,"open",NULL,0);
    XtAddCallback(_open,XmNactivateCallback,OpenCb,this);
    XtManageChild(_open);
    
    _save=XmCreatePushButtonGadget(_popupMenu,"save",NULL,0);
    XtAddCallback(_save,XmNactivateCallback,SaveCb,this);
    XtManageChild(_save);
    
    _properties=XmCreatePushButtonGadget(_popupMenu,"properties",NULL,0);
    XtAddCallback(_properties,XmNactivateCallback,PropertiesCb,this);
    XtManageChild(_properties);
    
    initializePopupMenu();
}


void XFE_ReadAttachPanel::setPaneHeight(int height)
{
    if (!getBaseWidget() || height<1 || height==getPaneHeight())
        return;

    int wasShown=isShown();

    // if panel is shown, we need to hide/show to force XmPanedWindow to
    // notice the height change
    if (wasShown)
        hide();

    // XmPaned window hack to set height of a pane
    XtVaSetValues(getBaseWidget(),
                  XmNpaneMinimum, height,
                  XmNpaneMaximum, height,
                  NULL);
    if (wasShown)
        show();
    
    XtVaSetValues(getBaseWidget(),
                  XmNpaneMinimum, 1,
                  XmNpaneMaximum, 10000,
                  NULL);
}

int XFE_ReadAttachPanel::getPaneHeight()
{
    if (!getBaseWidget())
        return 0;
    
    Dimension height;
    
    XtVaGetValues(getBaseWidget(),
                  XmNheight, &height,
                  NULL);
    
    return (int)height;
}

void XFE_ReadAttachPanel::addAttachments(MSG_Pane *pane,MSG_AttachmentData* data)
{
    // adopt storage of backend attachment list, free in removeAllAttachments
    _pane=pane;
    _attachments=data;
    
    MSG_AttachmentData* tmp;
    for (tmp = data ; tmp->url ; tmp++) {
        char *itemLabel=NULL;

        // Hack around back-end not naming the sent v-card. Use
        // description field: "Card for ...", to provide some
        // uniqueness among received vcards.
        if (tmp->real_name && XP_STRCASECMP(tmp->real_name,"vcard.vcf")==0 &&
            tmp->description && strlen(tmp->description)>0) {
            itemLabel=(char*)XP_ALLOC(strlen(tmp->description)+4+1);
            sprintf(itemLabel,"%s.vcf",tmp->description);
        }

        // find a label for the attachment
        else if (tmp->real_name && strlen(tmp->real_name)>0) {
            itemLabel=XP_STRDUP(tmp->real_name);
        } else if (tmp->description && strlen(tmp->description)>0) {
            itemLabel=XP_STRDUP(tmp->description);
        } else if (tmp->real_type && strlen(tmp->real_type)>0) {
            itemLabel=XP_STRDUP(tmp->real_type);
        } else {
            itemLabel=XP_STRDUP("attachment");
        }
        // translate problem characters in label to '_'
        char *s=itemLabel;
        while (*s) {
            if (*s==' ' || *s=='/' || *s=='\\')
                *s='_';
            s++;
        }

        XFE_AttachPanelItem *item = new
            XFE_AttachPanelItem(this,
                                tmp->url,
                                itemLabel,
                                tmp->real_type);
        addItem(item);
        // add to drag handler
        if (_attachDrag && item->image()) {
            _attachDrag->addDragWidget(item->image());
        }
        
        if (itemLabel)
            XP_FREE(itemLabel);
    }

    // select first attachment by default
    if (_numItems>0)
        selectItem(_items[0]);
}

void XFE_ReadAttachPanel::removeAllAttachments()
{
    removeAllItems();

    if (_pane && _attachments) {
        MSG_FreeAttachmentList(_pane, _attachments);
        _attachments=NULL;
    }

    // pop down attachment properties dialog, if it's managed
    if (_propertiesDialog)
        XtUnmanageChild(_propertiesDialog);

    // unlock frame, just in case this call caused an in-progress
    // drag to cancel without exiting fe_SaveSynchronousURL()
    if (_attachDrag)
        _attachDrag->unlockFrame();
}

void XFE_ReadAttachPanel::updateSelectionUI()
{
    if (!_popupMenu)
        return;

    if (_currentSelection) {
        XtSetSensitive(_open,TRUE);
        XtSetSensitive(_save,TRUE);
        XtSetSensitive(_properties,TRUE);
    }
    else {
        XtSetSensitive(_open,FALSE);
        XtSetSensitive(_save,FALSE);
        XtSetSensitive(_properties,FALSE);
    }

    // if attachment properties dialog is already being displayed, update it.
    if (_propertiesDialog && XtIsManaged(_propertiesDialog)) {
        if (_currentSelection)
            displayAttachmentProperties(currentSelectionPos());
    }    
}

void XFE_ReadAttachPanel::displayAttachmentProperties(int pos)
{
    if (pos<0 || pos>=_numItems)
        return;

    if (!_propertiesDialog) {
        // create properties dialog
        Arg args[20];
        int n;

        Visual *v=0;
        Colormap cmap=0;
        Cardinal depth=0;

        Widget shell=getBaseWidget();
        while (!XtIsShell(shell)) shell=XtParent(shell);
        XtVaGetValues(shell,XtNvisual,&v,XtNcolormap,&cmap,XtNdepth,&depth,NULL);

        Pixel bg;
        XtVaGetValues(getBaseWidget(),XmNbackground,&bg,NULL);
        fe_icon attachIcon={ 0 };
        fe_MakeIcon(_context,bg, &attachIcon, NULL,
		M_ToggleAttach.width, M_ToggleAttach.height,
		M_ToggleAttach.mono_bits, M_ToggleAttach.color_bits, M_ToggleAttach.mask_bits,
		FALSE);

        n=0;
        XtSetArg(args[n],XmNvisual,v); n++;
        XtSetArg(args[n],XmNdepth,depth); n++;
        XtSetArg(args[n],XmNcolormap,cmap); n++;
        XtSetArg(args[n],XmNdeleteResponse,XmUNMAP);n++;
        XtSetArg(args[n],XmNautoUnmanage,TRUE);n++;
        XtSetArg(args[n],XmNdialogStyle,XmDIALOG_MODELESS);n++;
        XtSetArg(args[n],XmNmwmFunctions,MWM_FUNC_MOVE|MWM_FUNC_CLOSE);n++;
        XtSetArg(args[n],XmNmwmDecorations,MWM_DECOR_BORDER|MWM_DECOR_TITLE|MWM_DECOR_MENU);n++;
        if (attachIcon.pixmap) {
            XtSetArg(args[n],XmNsymbolPixmap,attachIcon.pixmap);n++;
        }
        _propertiesDialog=XmCreateTemplateDialog(getBaseWidget(),"attachmentProps",args,n);
        fe_HackDialogTranslations(_propertiesDialog);
        fe_NukeBackingStore(_propertiesDialog);

        Widget form=XmCreateForm(_propertiesDialog,"form",NULL,0);
        XtManageChild(form);
        
        n=0;
        XtSetArg(args[n],XmNpacking,XmPACK_COLUMN); n++;
        XtSetArg(args[n],XmNnumColumns,1); n++;
        XtSetArg(args[n],XmNisAligned,FALSE); n++;
        XtSetArg(args[n],XmNalignment,XmVERTICAL); n++;
        XtSetArg(args[n],XmNleftAttachment,XmATTACH_FORM); n++;        
        XtSetArg(args[n],XmNtopAttachment,XmATTACH_FORM); n++;        
        XtSetArg(args[n],XmNbottomAttachment,XmATTACH_FORM); n++;        
        Widget labelRC=XmCreateRowColumn(form,"labelRC",args,n);
        XtManageChild(labelRC);

        n=0;
        XtSetArg(args[n],XmNalignment,XmALIGNMENT_END);n++;
        _nameLabel=XmCreateLabelGadget(labelRC,"nameLabel",args,n);        
        _typeLabel=XmCreateLabelGadget(labelRC,"typeLabel",args,n);
        _encLabel=XmCreateLabelGadget(labelRC,"encLabel",args,n);
        _descLabel=XmCreateLabelGadget(labelRC,"descLabel",args,n);

        n=0;
        XtSetArg(args[n],XmNpacking,XmPACK_COLUMN); n++;
        XtSetArg(args[n],XmNnumColumns,1); n++;
        XtSetArg(args[n],XmNisAligned,FALSE); n++;
        XtSetArg(args[n],XmNalignment,XmVERTICAL); n++;
        XtSetArg(args[n],XmNleftAttachment,XmATTACH_WIDGET); n++;        
        XtSetArg(args[n],XmNleftWidget,labelRC); n++;        
        XtSetArg(args[n],XmNrightAttachment,XmATTACH_FORM); n++;        
        XtSetArg(args[n],XmNtopAttachment,XmATTACH_FORM); n++;        
        XtSetArg(args[n],XmNbottomAttachment,XmATTACH_FORM); n++;        
        Widget valueRC=XmCreateRowColumn(form,"valueRC",args,n);
        XtManageChild(valueRC);

        n=0;
        XtSetArg(args[n],XmNalignment,XmALIGNMENT_BEGINNING);n++;
        _nameValue=XmCreateLabelGadget(valueRC,"nameValue",args,n);
        _typeValue=XmCreateLabelGadget(valueRC,"typeValue",args,n);
        _encValue=XmCreateLabelGadget(valueRC,"encValue",args,n);
        _descValue=XmCreateLabelGadget(valueRC,"descValue",args,n);
    }
    
    // set value of data fields, manage/unmanage relevent fields
    XmString xs;

    // file name
    char *name=_attachments[pos].real_name;
    if (name) {
        xs=XmStringCreateLocalized(name);
        XtVaSetValues(_nameValue,XmNlabelString,xs,NULL);
        XmStringFree(xs);
        XtManageChild(_nameLabel);
        XtManageChild(_nameValue);
    }
    else {
        XtUnmanageChild(_nameLabel);
        XtUnmanageChild(_nameValue);
    }

    // mime type
    char *type=_attachments[pos].real_type;
    if (type) {
        xs=XmStringCreateLocalized(type);
        XtVaSetValues(_typeValue,XmNlabelString,xs,NULL);
        XmStringFree(xs);
        XtManageChild(_typeLabel);
        XtManageChild(_typeValue);
    }
    else {
        XtUnmanageChild(_typeLabel);
        XtUnmanageChild(_typeValue);
    }

    // encoding
    char *enc=_attachments[pos].real_encoding;
    if (enc) {
        xs=XmStringCreateLocalized(enc);
        XtVaSetValues(_encValue,XmNlabelString,xs,NULL);
        XmStringFree(xs);
        XtManageChild(_encLabel);
        XtManageChild(_encValue);
    }
    else {
        XtUnmanageChild(_encLabel);
        XtUnmanageChild(_encValue);
    }

    // long description
    char *desc=_attachments[pos].description;
    if (desc) {
        xs=XmStringCreateLocalized(desc);
        XtVaSetValues(_descValue,XmNlabelString,xs,NULL);
        XmStringFree(xs);
        XtManageChild(_descLabel);
        XtManageChild(_descValue);
    }
    else {
        XtUnmanageChild(_descLabel);
        XtUnmanageChild(_descValue);
    }

    // manage widget if not already managed
    XtManageChild(_propertiesDialog);
}

// internal callback methods

void XFE_ReadAttachPanel::doubleClickCb(int pos)
{
    // ensure clicked item is selected
    if (pos!=currentSelectionPos() && _items && pos >=0 && pos < _numItems)
        selectItem(_items[pos]);

    openCb();
}


void XFE_ReadAttachPanel::openCb()
{
    if (!currentSelection())
        return;

    const char *selData=currentSelection()->data();
    if (!selData || strlen(selData)==0)
        return;

    URL_Struct *url = NET_CreateURLStruct (selData,NET_DONT_RELOAD);
    if (!MSG_RequiresBrowserWindow(url->address))
	fe_GetURL(_context,url,FALSE);
    else
        fe_MakeWindow(XtParent(CONTEXT_WIDGET (_context)), _context, url, NULL,
                      MWContextBrowser, FALSE);

    fe_UserActivity(_context);
}

void XFE_ReadAttachPanel::saveCb()
{
    if (!currentSelection() || !currentSelection()->data())
        return;

    URL_Struct *url = NET_CreateURLStruct (currentSelection()->data(),NET_DONT_RELOAD);

    if (url)
        fe_SaveURL(_context,url);
}

void XFE_ReadAttachPanel::propertiesCb()
{
    int pos=currentSelectionPos();

    if (pos<0 || pos>=_numItems)
        return;

    displayAttachmentProperties(pos);
}

