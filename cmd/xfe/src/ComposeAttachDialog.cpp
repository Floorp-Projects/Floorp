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
   ComposeAttachDialog.cpp -- compose attachment dialogs
   Created: Alastair Gourlay(SGI) c/o Dora Hsu<dora@netscape.com>, 26 Nov 1996
 */



// Classes in this file:
//      XFE_ComposeAttachLocationDialog
//      XFE_ComposeAttachFileDialog
//

#include "ComposeAttachDialog.h"
#include "DesktopTypes.h"
#include <stdlib.h>
#include <Xm/XmAll.h>
#include "xfe.h"
#include "felocale.h"

#ifdef DEBUG_sgidev
#define XDEBUG(x) x
#else
#define XDEBUG(x)
#endif

//
// XFE_ComposeAttachLocationDialog
//

// callback stubs

void XFE_ComposeAttachLocationDialog::OkCb(Widget,XtPointer cd,XtPointer) {
    XFE_ComposeAttachLocationDialog *ad=(XFE_ComposeAttachLocationDialog*)cd;
    if (ad)
        ad->okCb();
}

void XFE_ComposeAttachLocationDialog::ClearCb(Widget,XtPointer cd,XtPointer) {
    XFE_ComposeAttachLocationDialog *ad=(XFE_ComposeAttachLocationDialog*)cd;
    if (ad)
        ad->clearCb();
}

void XFE_ComposeAttachLocationDialog::CancelCb(Widget,XtPointer cd,XtPointer) {
    XFE_ComposeAttachLocationDialog *ad=(XFE_ComposeAttachLocationDialog*)cd;
    if (ad)
        ad->cancelCb();
}

// constructor

XFE_ComposeAttachLocationDialog::XFE_ComposeAttachLocationDialog(XFE_ComposeAttachFolderView *folder)
    : XFE_Component()
{
    _attachFolder=folder;
    
    _parent=NULL;
    _dialog=NULL;
    _locationText=NULL;
}

XFE_ComposeAttachLocationDialog::~XFE_ComposeAttachLocationDialog()
{
}

void XFE_ComposeAttachLocationDialog::createWidgets(Widget parent)
{
    _parent=parent;

    Arg args[20];
    int n;

    Visual *v = 0;
    Colormap cmap = 0;
    Cardinal depth = 0;

    Widget shell=parent;
    while (!XtIsShell(shell)) shell=XtParent(shell);
    XtVaGetValues(shell,XtNvisual,&v,XtNcolormap,&cmap,XtNdepth,&depth,NULL);

    n=0;
    XtSetArg (args[n], XmNvisual, v); n++;
    XtSetArg (args[n], XmNdepth, depth); n++;
    XtSetArg (args[n], XmNcolormap, cmap); n++;
    XtSetArg (args[n], XmNautoUnmanage, False); n++;
    XtSetArg (args[n], XmNdeleteResponse, XmUNMAP); n++;
    XtSetArg (args[n], XmNdialogStyle, XmDIALOG_MODELESS); n++;
    _dialog = XmCreateMessageDialog (_parent, "location_popup", args, n);
    fe_UnmanageChild_safe(XmMessageBoxGetChild(_dialog,XmDIALOG_MESSAGE_LABEL));
    fe_UnmanageChild_safe(XmMessageBoxGetChild(_dialog,XmDIALOG_SYMBOL_LABEL));
    fe_UnmanageChild_safe(XmMessageBoxGetChild(_dialog,XmDIALOG_HELP_BUTTON));
    XtAddCallback(_dialog,XmNunmapCallback,CancelCb,(XtPointer)this);
    XtAddCallback(_dialog,XmNokCallback,OkCb,(XtPointer)this);
    XtAddCallback(_dialog,XmNcancelCallback,CancelCb,(XtPointer)this);

    Widget clearButton=XmCreatePushButtonGadget (_dialog,"clear",NULL,0);
    XtAddCallback(clearButton,XmNactivateCallback,ClearCb,(XtPointer)this);
    XtManageChild(clearButton);
    
    Widget form=XmCreateForm(_dialog,"form",NULL,0);

    Widget label=XmCreateLabelGadget(form,"label",NULL,0);
    XtVaSetValues(label,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNbottomAttachment, XmATTACH_NONE,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_FORM,
                  NULL);
    XtManageChild(label);
    
    Widget locationLabel=XmCreateLabelGadget(form,"locationLabel",NULL,0);
    XtVaSetValues(locationLabel,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, label,
                  XmNbottomAttachment, XmATTACH_NONE,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  NULL);
    XtManageChild(locationLabel);
    
    _locationText=fe_CreateTextField(form,"locationText",NULL,0);
    XtVaSetValues(_locationText,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, label,
                  XmNbottomAttachment, XmATTACH_NONE,
                  XmNleftAttachment, XmATTACH_WIDGET,
                  XmNleftWidget, locationLabel,
                  XmNrightAttachment, XmATTACH_FORM,
                  NULL);
    if (fe_globalData.nonterminal_text_translations)
        XtOverrideTranslations (_locationText, fe_globalData.nonterminal_text_translations);
    XtAddCallback(_locationText,XmNactivateCallback,OkCb,(XtPointer)this);
    XtManageChild(_locationText);
    
    Dimension height;
    XtVaGetValues(_locationText,XmNheight,&height,NULL);
    XtVaSetValues(locationLabel,XmNheight,height,NULL);

    XtManageChild(form);
    
    fe_HackDialogTranslations(form);

    XtVaSetValues(_dialog,XmNinitialFocus,form,NULL);

    setBaseWidget(_dialog);
}

void  XFE_ComposeAttachLocationDialog::show()
{
    XFE_Component::show();
    if (_dialog)
        XMapRaised(XtDisplay(_dialog),XtWindow(XtParent(_dialog)));
}

//
// callback methods
//

void XFE_ComposeAttachLocationDialog::okCb()
{
    char *url=fe_GetTextField(_locationText);

    if (!url || strlen(url)==0) {
        XtFree(url);
        return;
    }
    
    // check for Netscape desktop file
    char *desktopUrl=NULL;
    char *attachUrl=url;
    XFE_DesktopTypeTranslate(url,&desktopUrl,FALSE);        
    if (desktopUrl && strlen(desktopUrl)>0)
        attachUrl=desktopUrl;
        
    // validate final url and attach
    if (XFE_ComposeAttachFolderView::validateAttachment(_dialog,attachUrl)) {
        _attachFolder->addAttachment(attachUrl);
        _attachFolder->scrollToItem(_attachFolder->numAttachments()-1);
        hide();
    }
    
    if (desktopUrl)
        XP_FREE(desktopUrl);
    
    if (url)
        XtFree(url);
}

void XFE_ComposeAttachLocationDialog::clearCb()
{
    fe_SetTextFieldAndCallBack(_locationText,"");
    XmProcessTraversal(_locationText,XmTRAVERSE_CURRENT);
}

void XFE_ComposeAttachLocationDialog::cancelCb()
{
    hide();
}




//
// XFE_ComposeAttachFileDialog
//

// callback stubs

void XFE_ComposeAttachFileDialog::OkCb(Widget,XtPointer cd,XtPointer cb) {
    XFE_ComposeAttachFileDialog *ad=(XFE_ComposeAttachFileDialog*)cd;
    if (ad) {
        char *filename=NULL;
        XmFileSelectionBoxCallbackStruct *fcb=(XmFileSelectionBoxCallbackStruct*)cb;
        if (fcb && fcb->value)
            XmStringGetLtoR(fcb->value,XmSTRING_DEFAULT_CHARSET,&filename);

        ad->okCb(filename);

        if (filename)
            XtFree(filename);
    }
}

void XFE_ComposeAttachFileDialog::CancelCb(Widget,XtPointer cd,XtPointer) {
    XFE_ComposeAttachFileDialog *ad=(XFE_ComposeAttachFileDialog*)cd;
    if (ad)
        ad->cancelCb();
}

void XFE_ComposeAttachFileDialog::SetFileAttachBinaryCb(Widget widget,XtPointer cd,XtPointer) {
    XFE_ComposeAttachFileDialog *ad=(XFE_ComposeAttachFileDialog*)cd;
#ifdef DEBUG
	printf("XFE_ComposeAttachFileDialog::SetFileAttachBinaryCb\n");
#endif
    if (ad)
        ad->setFileAttachBinaryCb(widget);
}


// constructor

XFE_ComposeAttachFileDialog::XFE_ComposeAttachFileDialog(XFE_ComposeAttachFolderView *folder)
    : XFE_Component()
{
    _attachFolder=folder;
    
    _parent=NULL;
    _dialog=NULL;
    _attachEncodingMenu=NULL;
    _attachBinaryButton=NULL;
}

XFE_ComposeAttachFileDialog::~XFE_ComposeAttachFileDialog()
{
}

void XFE_ComposeAttachFileDialog::createWidgets(Widget parent)
{
    _parent=parent;
    _dialog=_parent;

    Arg args[20];
    int n;

    Visual *v = 0;
    Colormap cmap = 0;
    Cardinal depth = 0;

    Widget shell=parent;
    while (!XtIsShell(shell)) shell=XtParent(shell);
    XtVaGetValues(shell,XtNvisual,&v,XtNcolormap,&cmap,XtNdepth,&depth,NULL);

    n=0;
    XtSetArg (args[n], XmNvisual, v); n++;
    XtSetArg (args[n], XmNdepth, depth); n++;
    XtSetArg (args[n], XmNcolormap, cmap); n++;
    XtSetArg(args[n],XmNdeleteResponse,XmUNMAP);n++;
    XtSetArg(args[n],XmNdialogStyle,XmDIALOG_MODELESS);n++;
    XtSetArg(args[n],XmNfileTypeMask,XmFILE_REGULAR);n++;
    _dialog=fe_CreateFileSelectionDialog(_parent,"fileBrowser",args,n);
#ifdef NO_HELP
    fe_UnmanageChild_safe(XmSelectionBoxGetChild(_dialog,XmDIALOG_HELP_BUTTON));
#endif

    XtAddCallback(_dialog,XmNokCallback,OkCb,(XtPointer)this);
    XtAddCallback(_dialog,XmNcancelCallback,CancelCb,(XtPointer)this);
    XtAddCallback(_dialog,XmNunmapCallback,CancelCb,(XtPointer)this);

    // create selector for attachment transfer encoding
    n=0;
    XtSetArg(args[n],XmNshadowType,XmSHADOW_ETCHED_IN);n++;
    Widget optionFrame=XmCreateFrame(_dialog,"encodingFrame",args,n);
    
    n=0;
    XtSetArg(args[n],XmNvisual,v);n++;
    XtSetArg(args[n],XmNdepth,depth);n++;
    XtSetArg(args[n],XmNcolormap,cmap);n++;
    Widget optionPopup=XmCreatePulldownMenu(optionFrame,"optionPopup",args,n);

    Widget attachAutoDetectButton=XmCreatePushButtonGadget(optionPopup,"attachAutoDetect",NULL,0);
	XtAddCallback(attachAutoDetectButton, XmNactivateCallback, SetFileAttachBinaryCb, (XtPointer)this);
    XtManageChild(attachAutoDetectButton);
    _attachBinaryButton=XmCreatePushButtonGadget(optionPopup,"attachBinary",NULL,0);
	XtAddCallback(_attachBinaryButton, XmNactivateCallback, SetFileAttachBinaryCb, (XtPointer)this);
    XtManageChild(_attachBinaryButton);

    n=0;
    XtSetArg(args[n],XmNsubMenuId,optionPopup);n++;
	if (fe_globalPrefs.file_attach_binary) {
    	XtSetArg(args[n],XmNmenuHistory,_attachBinaryButton);n++;
	}
	else {
    	XtSetArg(args[n],XmNmenuHistory,attachAutoDetectButton);n++;
	}
    _attachEncodingMenu=XmCreateOptionMenu(optionFrame,"optionMenu",args,n);
    XtManageChild(_attachEncodingMenu);

    XtManageChild(optionFrame);

    fe_HackDialogTranslations(_dialog);
    fe_NukeBackingStore(_dialog);
    
    setBaseWidget(_dialog);
}

void  XFE_ComposeAttachFileDialog::show()
{
    XFE_Component::show();
    if (_dialog)
        XMapRaised(XtDisplay(_dialog),XtWindow(XtParent(_dialog)));
}

//
// callback methods
//

void XFE_ComposeAttachFileDialog::okCb(const char *filename)
{
    if (!filename || strlen(filename)==0)
        return;
    
    // check for Netscape desktop file
    char *desktopUrl=NULL;
    const char *attachUrl=filename;
    XFE_DesktopTypeTranslate(filename,&desktopUrl,FALSE);
    if (desktopUrl && strlen(desktopUrl)>0)
        attachUrl=desktopUrl;

    // validate final url and attach
    if (XFE_ComposeAttachFolderView::validateAttachment(_dialog,attachUrl)) {
        // check whether user selected binary as the tranfer encoding
        Boolean attach_binary=FALSE;
        if (_attachEncodingMenu && _attachBinaryButton) {
            Widget attachEncodingWidget=NULL;
            XtVaGetValues(_attachEncodingMenu,XmNmenuHistory,&attachEncodingWidget,NULL);
            attach_binary=(attachEncodingWidget==_attachBinaryButton)? TRUE:FALSE;
        }
        _attachFolder->addAttachment(attachUrl,FALSE,attach_binary);
        _attachFolder->scrollToItem(_attachFolder->numAttachments()-1);
        hide();
    }

    if (desktopUrl)
        XP_FREE(desktopUrl);
}

void XFE_ComposeAttachFileDialog::cancelCb()
{
    hide();
}

void XFE_ComposeAttachFileDialog::setFileAttachBinaryCb(XtPointer widget)
{
	XP_Bool attach_binary;

	if ((Widget)widget == _attachBinaryButton)
		attach_binary = True;
	else
		attach_binary = False;
	fe_globalPrefs.file_attach_binary = attach_binary;
}

