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
   ComposeAttachFolderView.h -- class definitions for compose attachment folder
   Created: Alastair Gourlay(SGI) c/o Dora Hsu<dora@netscape.com>, 26 Nov 1996
 */



#ifndef _COMPOSE_ATTACH_FOLDER_VIEW_H
#define _COMPOSE_ATTACH_FOLDER_VIEW_H

//
// ComposeAttachFolderView.h
//
// Classes in this file:
//      XFE_ComposeAttachFolderView
//      XFE_ComposeAttachPanel
//      XFE_ComposeAttachDrop
//

#include "MNView.h"
#include "AttachPanel.h"
#include "DragDrop.h"
#include "ComposeAttachDialog.h"
#include "PopupMenu.h"

class XFE_ComposeAttachDrop;
class XFE_ComposeAttachPanel;
class XFE_ComposeAttachLocationDialog;
class XFE_ComposeAttachFileDialog;

//
// XFE_ComposeAttachFolderView 
//

class XFE_ComposeAttachFolderView : public XFE_MNView
{
public:
    XFE_ComposeAttachFolderView(
               XFE_Component *toplevel_component, XFE_View *parent_view,
               MSG_Pane *p = NULL, MWContext *context = NULL);
    virtual ~XFE_ComposeAttachFolderView();

    // widgets
    virtual void createWidgets(Widget);
    virtual void show();
    virtual void hide();
    virtual void updateSelectionUI();

    // commands
    virtual Boolean isCommandEnabled(CommandType,void* calldata=NULL,
								   XFE_CommandInfo* i = NULL);
    virtual Boolean handlesCommand(CommandType,void* calldata=NULL,
								   XFE_CommandInfo* i = NULL);
    virtual void doCommand(CommandType,void* calldata=NULL,
								   XFE_CommandInfo* i = NULL);

    // attachments
    void folderVisible(int);
    int maxAttachments() { return _maxAttachments; };
    int numAttachments() { return _numAttachments; };
    int addAttachment(const char*,int pre_existing=FALSE,Boolean attach_binary=FALSE);
    int addAttachments(const char**,int,int pre_existing=FALSE,Boolean attach_binary=FALSE);
    void deleteAttachment(int);
    void updateAttachments();
    void addExistingAttachments();
    void scrollToItem(int);
    char *parseItemLabel(const char*);

    // entry point used by XFE_TaskBarDrop for mail+news drops
    static int validateAttachment(Widget,const char*);

    // entry points for external objects to control attachments
    void attachFile();
    void attachLocation();
    void deleteAttach();
    void openAttachment(int);

#if !defined(USE_MOTIF_DND)
    // internal drag management
    static void AttachDropCb(Widget,void*,fe_dnd_Event,fe_dnd_Source*,XEvent*);
    void attachDropCb(fe_dnd_Source*);
#endif /* USE_MOTIF_DND */

private:
    XFE_ComposeAttachPanel *_attachPanel;
    XFE_ComposeAttachLocationDialog *_attachLocationDialog;
    XFE_ComposeAttachFileDialog *_attachFileDialog;
    XFE_PopupMenu *_xfePopup;
    
    int _addedExistingAttachments;
    
    MWContext* _context;

    int _folderVisible;
    int _numAttachments;
    struct MSG_AttachmentData *_attachments;

    Widget _deleteAttachButton;
    
    static char *_lastAttachmentType;
    static const int _maxAttachments;

    // internal utilities
    int verifySafeToAttach();

#if !defined(USE_MOTIF_DND)
    void processBookmarkDrop(fe_dnd_Source*);
    void processHistoryDrop(fe_dnd_Source*);
    void processMessageDrop(fe_dnd_Source*);    
#endif /* USE_MOTIF_DND */
};


//
// XFE_ComposeAttachPanel
//

class XFE_ComposeAttachPanel : public XFE_AttachPanel {
public:
    XFE_ComposeAttachPanel(XFE_ComposeAttachFolderView*,MWContext*);
    virtual ~XFE_ComposeAttachPanel();

    // widgets
    void createWidgets(Widget);
    void show();
    void hide();
    void setSensitive(Boolean);

    // hooks for XFE_DropAttach
    int dropAttachment(const char *);
    int dropAttachments(const char**,int);
protected:
    XFE_ComposeAttachFolderView *_attachFolder;
    Widget _delete;
    XFE_ComposeAttachDrop *_dropSite;

    // AttachPanel methods
    void doubleClickCb(int);
    
    // internal UI management
    virtual void updateSelectionUI();    
private:
};


//
// XFE_ComposeAttachDrop
//

class XFE_ComposeAttachDrop : public XFE_DropNetscape
{
public:
    XFE_ComposeAttachDrop(Widget,XFE_ComposeAttachPanel*);
    virtual ~XFE_ComposeAttachDrop();
    
protected:
    XFE_ComposeAttachPanel *_attachPanel;

    // base class methods changed
    void operations(); // override to set _operations
    void targets(); // override to augment XFE_DropFile _targets
    int processTargets(Atom*,const char**,int);
private:
};


#endif // _COMPOSE_ATTACH_FOLDER_VIEW_H
