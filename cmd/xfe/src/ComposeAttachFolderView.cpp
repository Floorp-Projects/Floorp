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
   ComposeAttachFolderView.cpp -- compose window folder containing attachment panel
   Created: Alastair Gourlay(SGI) c/o Dora Hsu<dora@netscape.com>, 26 Nov 1996
 */



// Classes in this file:
//      XFE_ComposeAttachFolderView
//      XFE_ComposeAttachPanel
//      XFE_ComposeAttachDrop
//

#include "ComposeAttachFolderView.h"

#include <stdlib.h>
#include <unistd.h>

#include "xfe.h"
#include "net.h"
#include "msgcom.h"
#include "xp_mem.h"
#include <xpgetstr.h>
#include <Xm/Xm.h>
#include <Xm/XmP.h> // need widget-writer call _XmSetDestination()

#include <Xm/XmAll.h>
#include <Xfe/Xfe.h>

#include "ThreadView.h"
#include "BrowserFrame.h"
#include "BookmarkView.h"
#include "HistoryView.h"

// from lib/libmsg
extern "C" XP_Bool MSG_RequiresMailMsgWindow(const char*);
extern "C" XP_Bool MSG_RequiresNewsMsgWindow(const char*);

// Use API from XmP.h - moderately naughty, but implicitly supported for
// widget-writers and happy hackers everywhere.
extern "C" void _XmSetDestination(Display*,Widget);

static int IsWebJumper(const char*);

//
// XFE_ComposeAttachFolderView
//


#ifdef DEBUG_sgidev
#define XDEBUG(x) x
#else
#define XDEBUG(x)
#endif

// error messages

extern int XFE_INVALID_FILE_ATTACHMENT_IS_A_DIRECTORY;
extern int XFE_INVALID_FILE_ATTACHMENT_NOT_READABLE;
extern int XFE_INVALID_FILE_ATTACHMENT_DOESNT_EXIST;
extern int XFE_MN_TOO_MANY_ATTACHMENTS;
extern int XFE_MN_ITEM_ALREADY_ATTACHED;
extern int XFE_MN_DELIVERY_IN_PROGRESS;
extern int XFE_MN_INVALID_ATTACH_URL;

// popup menu for attach panel
static MenuSpec attachPopupMenuSpec[] = {
  { xfeCmdAttachFile,			PUSHBUTTON },
  { xfeCmdAttachWebPage,		PUSHBUTTON },
  { xfeCmdDeleteAttachment,		PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdAttachAddressBookCard,	TOGGLEBUTTON },
  { NULL }
};

// intialize static members

char *XFE_ComposeAttachFolderView::_lastAttachmentType=NULL;
const int XFE_ComposeAttachFolderView::_maxAttachments=128;

#if !defined(USE_MOTIF_DND)

// callback stubs

void XFE_ComposeAttachFolderView::AttachDropCb(Widget,void* cd,fe_dnd_Event type,fe_dnd_Source *source,XEvent*) {
    XFE_ComposeAttachFolderView *ad=(XFE_ComposeAttachFolderView*)cd;
    
    if (type==FE_DND_DROP && ad && source)
        ad->attachDropCb(source);
}
#endif /* USE_MOTIF_DND */

// constructor

XFE_ComposeAttachFolderView::XFE_ComposeAttachFolderView(
                                 XFE_Component *toplevel_component,
                                 XFE_View *parent_view,
                                 MSG_Pane *p,
                                 MWContext *context) 
    : XFE_MNView(toplevel_component, parent_view, context, p)
{
    setParent(parent_view);
    _attachPanel=NULL;
    _attachLocationDialog=NULL;
    _attachFileDialog=NULL;
    _xfePopup=NULL;
    _context=context;
    _attachments=new struct MSG_AttachmentData[_maxAttachments];
    _numAttachments=0;

    _folderVisible=FALSE;
    _deleteAttachButton=NULL;
    _addedExistingAttachments=FALSE;
}

// destructor

XFE_ComposeAttachFolderView::~XFE_ComposeAttachFolderView()
{
    if (_attachPanel)
	delete _attachPanel;

    if (_attachLocationDialog)
        delete _attachLocationDialog;
    
    if (_attachFileDialog)
        delete _attachFileDialog;
    
    if (_xfePopup)
        delete _xfePopup;
    
    if (_attachments) {
        for (int i=0;i<_numAttachments;i++)
            if (_attachments[i].url)
                XP_FREE(_attachments[i].url);

        delete [] _attachments;
    }
}

// create UI

void XFE_ComposeAttachFolderView::createWidgets(Widget parent)
{
    // create attachment folder form
    Widget form=XmCreateForm(parent,"attachForm",NULL,0);

    // create attachment panel
    _attachPanel=new XFE_ComposeAttachPanel(this,_context);
    _attachPanel->createWidgets(form);
    _attachPanel->show();
    XtVaSetValues(_attachPanel->getBaseWidget(),
                  XmNleftAttachment,XmATTACH_FORM,
                  XmNrightAttachment,XmATTACH_FORM,
                  XmNtopAttachment,XmATTACH_FORM,
                  XmNbottomAttachment,XmATTACH_FORM,
                  NULL);

#if !defined(USE_MOTIF_DND)
    // create drop site for internal drop (message,news etc.)
    fe_dnd_CreateDrop(_attachPanel->getBaseWidget(),AttachDropCb,this);
#endif /* USE_MOTIF_DND */

    // create popup menu
    _xfePopup = new XFE_PopupMenu("popup",(XFE_Frame*)m_toplevel,_attachPanel->pane(),NULL);
    _xfePopup->addMenuSpec(attachPopupMenuSpec);

	// Remove the osfLeft and osfRight translations of the popup's children.
	// For bug 71620.  See the bug description for details.  Basically, for
	// some strange reason, motif assumes that either a left or a right 
	// naviation widget exists for this popup menu and core dumps trying 
	// to dereference a NULL widget.
    _xfePopup->removeLeftRightTranslations();

    setBaseWidget(form);

    // Register this widget to have a tooltip.
    fe_WidgetAddToolTips(form);
    
    // add existing attachments to the panel (e.g. for Forward)
    addExistingAttachments();
}

void XFE_ComposeAttachFolderView::show()
{
    XFE_MNView::show();
    
    // enable attachment panel's drop site
    if (_attachPanel)
        _attachPanel->show();
}

void XFE_ComposeAttachFolderView::hide()
{
    XFE_MNView::hide();
    
    // disable attachment panel's drop site
    if (_attachPanel)
        _attachPanel->hide();
}

// track viewable state of attachment folder tab
void XFE_ComposeAttachFolderView::folderVisible(int visible)
{
    _folderVisible=visible;
    
    if (visible) {
        // make sure pre-exisiting attachments are added
        addExistingAttachments();

        // take Motif 'active' status away from Subject/text area so
        // that xfeCmdDelete gets routed appropriately.
        _XmSetDestination(XtDisplay(getBaseWidget()),NULL);
    }
}

Boolean
XFE_ComposeAttachFolderView::isCommandEnabled(CommandType command, void*,
                                              XFE_CommandInfo*)
{
    // return True for  all the commands that are handled in this class
    if (command==xfeCmdAttachFile ||
        command==xfeCmdAttachWebPage || 
 	command==xfeCmdShowPopup) { 
        return True;
    }

    if ((command==xfeCmdDelete || command==xfeCmdDeleteAttachment) &&
        _folderVisible &&
        _attachPanel &&
        _attachPanel->currentSelection()) {
        return True;
    }
    
    // If we don't hande any command here, return False. Don't call parent
    // because the container will decide who to dispatch this command to
    return False;
}

Boolean
XFE_ComposeAttachFolderView::handlesCommand(CommandType command, void*,
                                            XFE_CommandInfo*)
{
    // return True for  all the commands that are handled in this class
    if (command==xfeCmdAttachFile ||
        command==xfeCmdAttachWebPage ||
        command==xfeCmdDeleteAttachment ||
        command==xfeCmdDelete ||
        command==xfeCmdShowPopup) {
        return True;
    }

    // If we don't hande any command here, return False. Don't call parent
    // because the container will decide who to dispatch this command to
    return False;
    
}

void XFE_ComposeAttachFolderView::doCommand(CommandType command, void*,XFE_CommandInfo* info)
{
    if (command==xfeCmdAttachFile) {
        attachFile();
    } else if (command==xfeCmdAttachWebPage) {
        attachLocation();
    } else if ((command==xfeCmdDelete || command==xfeCmdDeleteAttachment) &&
               _folderVisible) {
        deleteAttach();
    } else if (command==xfeCmdShowPopup) {
        // Handle popup menu
        XEvent *event = info->event;
        Widget w = XtWindowToWidget(event->xany.display, event->xany.window);

        if (w == NULL) w=getBaseWidget();

        if (_xfePopup) {
            _xfePopup->position(event);
            _xfePopup->show();
        }
    }
}


// update UI in response to selection change
void XFE_ComposeAttachFolderView::updateSelectionUI()
{
    // take Motif 'active' status away from Subject/text area so
    // that xfeCmdDelete gets routed appropriately.
    if (getBaseWidget())
        _XmSetDestination(XtDisplay(getBaseWidget()),NULL);

    // update Delete menu entry
    getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
}

//
// attachment management
//

void XFE_ComposeAttachFolderView::addExistingAttachments()
{
    // we're only going to do this once per window
    if (_addedExistingAttachments)
        return;
    
    // add pre-existing attachments to the icon panel (e.g. during msg forward)
    const struct MSG_AttachmentData *al=MSG_GetAttachmentList(getPane());
    if (al) {
        while(al->url) {
            addAttachment(al->url,TRUE); // set 'pre_exisiting' flag - don't add to internal list
            al++;
        }
    }
}

void XFE_ComposeAttachFolderView::updateAttachments()
{
    if (!getPane())
        return;

    // abort if attachment adding or delivery is in progress
    if (!verifySafeToAttach())
        return;

    // add to list
    _attachments[_numAttachments].url=NULL;
    MSG_SetAttachmentList(getPane(),_attachments);
}


int XFE_ComposeAttachFolderView::addAttachment(const char *itemData,int pre_existing,Boolean attach_binary)
{
    // abort if attachment adding or delivery is in progress
    // (and not adding a pre_exisiting attachment)
    if (!pre_existing && !verifySafeToAttach())
        return FALSE;

    int addStatus=FALSE;
    const char **items=new const char*[1];
    items[0]=itemData;
    addStatus=addAttachments(items,1,pre_existing,attach_binary);
    delete items;
    
    return addStatus;
}

int XFE_ComposeAttachFolderView::addAttachments(const char **items,int numItems,int pre_existing,Boolean attach_binary)
{
    // lock out addition of existing attachments after first time through
    _addedExistingAttachments=TRUE;
    
    // abort if attachment adding or delivery is in progress
    // (and not adding pre_exisiting attachments)
    if (!pre_existing && !verifySafeToAttach())
        return FALSE;

    if (!items || numItems==0)
        return FALSE;

    int addStatus=FALSE;
    
    // desired type is NULL == as-is (used to read from format toggle buttons.)
    char *desiredType=NULL;

    // if an icon has focus, restore it when we remap the pane
    Widget focusWidget=XmGetFocusWidget(getBaseWidget());
    if (focusWidget && XtParent(focusWidget)!=_attachPanel->pane())
        focusWidget=NULL;
    
    _attachPanel->unmapPane();
    for (int i=0;i<numItems;i++) {
        // is there space in the list?
        if (_numAttachments>=_maxAttachments) {
            char *msg=PR_smprintf(XP_GetString(XFE_MN_TOO_MANY_ATTACHMENTS));
            if (msg) {
                fe_Alert_2(getBaseWidget(),msg);
                XP_FREE(msg);
            }
            break;
        }

        // is the item already attached?
        int duplicate=FALSE;
        for (int j=0;j<_numAttachments;j++) {
            if (strcmp(items[i],_attachments[j].url)==0) {
                char *msg=PR_smprintf(XP_GetString(XFE_MN_ITEM_ALREADY_ATTACHED),items[i]);
                if (msg) {
                    fe_Alert_2(getBaseWidget(),msg);
                    XP_FREE(msg);
                }
                duplicate=TRUE;
                break;
            }
        }
        if (duplicate)
            continue;

        // nyi - hack "addbook:add?vcard=" URL to be vcard data, and accept as attachment
        // (then turn back on dragging of addbook:add URL's in XFE_HTMLDrag)
        // WinFE does this already.

        // is it a valid attachment?
        if ((!pre_existing) && (!validateAttachment(getBaseWidget(),items[i])))
            continue;

        // at least one attachment was accepted, return success status
        addStatus=TRUE;
        
        // add to internal attachment list
        struct MSG_AttachmentData m = { 0 };
        m.url=XP_STRDUP(items[i]);
        m.desired_type=desiredType;
        if (attach_binary)
            m.real_type= "application/octet-stream";
        else if (IsWebJumper(m.url))
            m.real_type= "text/webjumper";
        else
            m.real_type=NULL;
        _attachments[_numAttachments]=m;
        _numAttachments++;

        char *itemLabel=parseItemLabel(items[i]);
        
        // add icon to attachment panel
        _attachPanel->addItem(items[i],itemLabel);
        XP_FREE(itemLabel);
    }

    // always select last-added item
    if (addStatus && _attachPanel->numItems()>0) {
        _attachPanel->selectItem(_attachPanel->items()[_attachPanel->numItems()-1]);
        if (!pre_existing) {
            focusWidget=_attachPanel->items()[_attachPanel->numItems()-1]->image();
        }
    }
    
    // update attachment panel
    _attachPanel->updateDisplay();
    _attachPanel->mapPane();

    // restore focus if we had it before the unmapPane()
    if (focusWidget)
        XmProcessTraversal(focusWidget,XmTRAVERSE_CURRENT);

    // update internal attachment list, if not adding pre-exisiting attachments
    if (addStatus && !pre_existing)
        updateAttachments();    

    // pop attachment folder to top, if not adding pre-exisiting attachments
    if (!pre_existing) {
        getParent()->doCommand(xfeCmdViewAttachments);
    }

    return addStatus;
}

void XFE_ComposeAttachFolderView::deleteAttachment(int pos)
{
    if (pos<0 || pos>=_numAttachments)
        return;

    // abort if attachment adding or delivery is in progress
    if (!verifySafeToAttach())
        return;
    
    if (_attachments[pos].url)
        XP_FREE(_attachments[pos].url);

    // remove from internal attachment list
    for (int i=pos;i<_numAttachments-1;i++)
        _attachments[i]=_attachments[i+1];    
    _numAttachments--;
    updateAttachments();
    
    // remove icon from attach panel
    if (_attachPanel) {
        _attachPanel->removeItem(pos);
        _attachPanel->updateDisplay();
    }
}


int XFE_ComposeAttachFolderView::validateAttachment(Widget widget,const char *url)
{
    if (!url)
        return FALSE;

    // strip off any file: prefix before validating
    const char *data=url;
    if (XP_STRNCASECMP(url,"file:",5)==0)
        data=url+5;
    
    if (strlen(data)==0)
        return FALSE;

    // accept only URL's that resolve to a document
    // reject mailto:, mailbox: folders etc.
    if (NET_URL_Type(data)!=0) {
        // reject addressbook add command, without error dialog
        if (XP_STRNCASECMP(url,"addbook:add?vcard=",18)==0)
            return FALSE;
        
        // accept regular address book cards
        if (XP_STRNCASECMP(url,"addbook:",8)==0)
            return TRUE;

        // accept anything we know how to display as a document
        if (MSG_RequiresMailMsgWindow(url) ||
            MSG_RequiresNewsMsgWindow(url) ||
            MSG_RequiresBrowserWindow(url))
            return TRUE;
        else {
            char *msg=PR_smprintf(XP_GetString(XFE_MN_INVALID_ATTACH_URL),data);
            if (msg) {
                fe_Alert_2(widget,msg);
                XP_FREE(msg);
            }
            return FALSE;
        }
    }
    
    // file must exist
    if (!fe_isFileExist((char*)data)) {
        char *msg=PR_smprintf(XP_GetString(XFE_INVALID_FILE_ATTACHMENT_DOESNT_EXIST),data);
        if (msg) {
            fe_Alert_2(widget,msg);
            XP_FREE(msg);
        }
        return FALSE;
    }

    // file must be readable
    if (!fe_isFileReadable((char*)data)) {
        char *msg=PR_smprintf(XP_GetString(XFE_INVALID_FILE_ATTACHMENT_NOT_READABLE),data);
        if (msg) {
            fe_Alert_2(widget,msg);
            XP_FREE(msg);
        }
        return FALSE;
    }

    // cannot attach directory
    if (fe_isDir((char*)data)) {
        char *msg=PR_smprintf(XP_GetString( XFE_INVALID_FILE_ATTACHMENT_IS_A_DIRECTORY ),data);
        if (msg) {
            fe_Alert_2(widget,msg);
            if (msg) XP_FREE(msg);
        }
        return FALSE;
    }

    return TRUE;
}

    
void XFE_ComposeAttachFolderView::scrollToItem(int pos)
{
    if (_attachPanel)
        _attachPanel->scrollToItem(pos);
}

// create short icon label from full path/url
char *XFE_ComposeAttachFolderView::parseItemLabel(const char *url)
{
    // (alastair) code taken from libmsg/msgsend.cpp - be nice just to call into libmsg
    // modified slightly to avoid bug of trailing / generating empty name
    // modified to return truncated label for mail/news URL's

    if (!url || strlen(url)==0)
        return XP_STRDUP("(null)");

    char *s;
    char *s2;
    char *s3;
    
    /* If we know the URL doesn't have a sensible file name in it,
       don't bother emitting a content-disposition. */
    if (!strncasecomp (url, "news:", 5))
        return XP_STRDUP("news:");
    if (!strncasecomp (url, "snews:", 6))
        return XP_STRDUP("snews:");    
    if (!strncasecomp (url, "mailbox:", 8))
        return XP_STRDUP("mailbox:");

    char *tmpLabel = XP_STRDUP(url);

    s=tmpLabel;
    /* remove trailing / or \ */
    int len=strlen(s);
    if (s[len-1]=='/' || s[len-1]=='\\')
        s[len-1]='\0';
    
    s2 = XP_STRCHR (s, ':');
    if (s2) s = s2 + 1;
    /* Take the part of the file name after the last / or \ */
    s2 = XP_STRRCHR (s, '/');
    if (s2) s = s2+1;
    s2 = XP_STRRCHR (s, '\\');
    if (s2) s = s2+1;

    /* if it's a non-file url, strip off any named anchors or search data */
    if (XP_STRNCASECMP(url,"file:",5)!=0 && url[0]!='/') {
        /* Now trim off any named anchors or search data. */
        s3 = XP_STRCHR (s, '?');
        if (s3) *s3 = 0;
        s3 = XP_STRCHR (s, '#');
        if (s3) *s3 = 0;
    }

    /* Now lose the %XX crap. */
    NET_UnEscape (s);

    char *retLabel=XP_STRDUP(s);
    XP_FREE(tmpLabel);
    
    return retLabel;
}


    

extern "C" XP_Bool MSG_DeliveryInProgress(MSG_Pane * composepane);

int XFE_ComposeAttachFolderView::verifySafeToAttach()
{
#if 0
    // this check is no longer needed - back end can handle
    // multiple simultaneous attachment operations
    if (MSG_DeliveryInProgress(getPane())) {
        char *msg=PR_smprintf(XP_GetString(XFE_MN_DELIVERY_IN_PROGRESS));
        if (msg) {
            fe_Alert_2(getBaseWidget(),msg);
            XP_FREE(msg);
        }
        return FALSE;
    }
#endif
    return TRUE;
}


//
// entry points for external objects to control attachments
//

void XFE_ComposeAttachFolderView::attachFile()
{
    if (_attachFileDialog==NULL) {
        _attachFileDialog=new XFE_ComposeAttachFileDialog(this);
        _attachFileDialog->createWidgets(getBaseWidget());
    }
    
    _attachFileDialog->show();
}

void XFE_ComposeAttachFolderView::attachLocation()
{
    if (_attachLocationDialog==NULL) {
        _attachLocationDialog=new XFE_ComposeAttachLocationDialog(this);
        _attachLocationDialog->createWidgets(getBaseWidget());
    }
    
    _attachLocationDialog->show();
}

void XFE_ComposeAttachFolderView::deleteAttach()
{
    int pos=_attachPanel->currentSelectionPos();

    if (pos>=0)
        deleteAttachment(pos);
}

void XFE_ComposeAttachFolderView::openAttachment(int pos)
{
    if (pos<0 || pos>=_attachPanel->numItems())
        return;

    // Disable preview of previously attached item. Need to resolve
    // inconsistency when attaching URL pointing to cgi. Preview
    // will reload the URL, and the return data may differ from the
    // data previously loaded by the mail back-end. i.e. it will
    // not be a useful preview.
#if 0
    // ensure clicked item is selected
    if (pos!=_attachPanel->currentSelectionPos() && _attachPanel->items())
        _attachPanel->selectItem(_attachPanel->items()[pos]);

    XFE_AttachPanelItem *item=_attachPanel->currentSelection();
    
    if (!item || !item->data())
        return;
    
    URL_Struct *url = NET_CreateURLStruct (item->data(),NET_DONT_RELOAD);
    if (!MSG_RequiresBrowserWindow(url->address))
        fe_GetURL(_context,url,FALSE);
    else
        fe_MakeWindow(XtParent(CONTEXT_WIDGET (_context)), _context, url, NULL,
                      MWContextBrowser, FALSE);

    fe_UserActivity(_context);
#endif
}


//
// XFE_ComposeAttachPanel
//

// constructor

XFE_ComposeAttachPanel::XFE_ComposeAttachPanel(XFE_ComposeAttachFolderView *folder,MWContext *context) :
    XFE_AttachPanel(context)
{
    _attachFolder=folder;
    _dropSite=NULL;
    _iconTranslations=fe_globalData.mailcompose_global_translations;
}

// destructor

XFE_ComposeAttachPanel::~XFE_ComposeAttachPanel()
{
    if (_dropSite)
        delete _dropSite;
}


// create Motif UI

void XFE_ComposeAttachPanel::createWidgets(Widget parent)
{
    // create panel UI

    XFE_AttachPanel::createWidgets(parent);

    // register drop site

    _dropSite=new XFE_ComposeAttachDrop(_topClip,this);
}

// manage button state

void XFE_ComposeAttachPanel::updateSelectionUI()
{
    // update attach folder UI - Delete menu item state depends on selection

    _attachFolder->updateSelectionUI();
}

// handle double-click - open selected attachment

void XFE_ComposeAttachPanel::doubleClickCb(int pos)
{
    _attachFolder->openAttachment(pos);
}

// panel management

void XFE_ComposeAttachPanel::show()
{
    XFE_AttachPanel::show();
    if (_dropSite) _dropSite->enable();
}

void XFE_ComposeAttachPanel::hide()
{
    XFE_AttachPanel::hide();
    if (_dropSite) _dropSite->disable();
}

void XFE_ComposeAttachPanel::setSensitive(Boolean sensitive)
{
    XFE_AttachPanel::setSensitive(sensitive);

    if (sensitive)
        if (_dropSite) _dropSite->enable();
    else
        if (_dropSite) _dropSite->disable();
}


// drop management - pass through to XFE_ComposeAttachFolderView

int XFE_ComposeAttachPanel::dropAttachments(const char **item,int numItems)
{
    int dropStatus=_attachFolder->addAttachments(item,numItems);

    // ensure last-added item is visible
    if (dropStatus)
        scrollToItem(_numItems-1);
    
    return dropStatus;
}

int XFE_ComposeAttachPanel::dropAttachment(const char *item)
{
    int dropStatus=_attachFolder->addAttachment(item);

    // ensure last item is visible
    // ensure last-added item is visible
    if (dropStatus)
        scrollToItem(_numItems-1);

    return dropStatus;
}

static int IsWebJumper(const char *url)
{    
    const char *filename=url;

    // make sure it's a file
    if (XP_STRNCASECMP(url,"file:",5)==0)
        filename=url+5;
    if (strlen(filename)==0 || filename[0]!='/')
        return FALSE;

    // check for SGI WebJumper
    FILE *fp;
    int retval=FALSE;
    
    if (fp=fopen(filename,"r")) {
        const int MAX_LENGTH=4000;
        char line[MAX_LENGTH+1];
        line[MAX_LENGTH]='\0';
        line[0]='\0'; // ensure string will be null-terminated
        fgets(line,MAX_LENGTH,fp);

        if (XFE_WebJumperDesktopType::isDesktopType(line)) {
            retval=TRUE;
        }
        
        fclose(fp);
    }
    return retval;
}
    

//
// XFE_ComposeAttachDrop
//

// constructor

XFE_ComposeAttachDrop::XFE_ComposeAttachDrop(Widget parent,XFE_ComposeAttachPanel *attach)
    : XFE_DropNetscape(parent)
{
    _attachPanel=attach;
}


// destructor

XFE_ComposeAttachDrop::~XFE_ComposeAttachDrop()
{
}

//
// methods below override DragFile defaults
//

// set _operations

void XFE_ComposeAttachDrop::operations()
{
    // restrict attachment drop site to copy-only
    _operations=(unsigned int)XmDROP_COPY;
}

// set _targets

void XFE_ComposeAttachDrop::targets()
{
    _numTargets=2;
    _targets=new Atom[_numTargets];

    _targets[0]=_XA_NETSCAPE_URL;
    _targets[1]=XA_STRING;

    acceptFileTargets();
}

// process drop

int XFE_ComposeAttachDrop::processTargets(Atom *targets,const char **data,int numItems)
{
    XDEBUG(printf("XFE_ComposeAttachDrop::processTargets()\n"));
    
    if (!targets || !data || numItems==0)
        return FALSE;

    // pass dropped data to attachment panel

    char **dropInfo=new char*[numItems];
    int numDropInfo=0;
    int i;
    
    for (i=0;i<numItems;i++) {
        if (targets[i]==None || data[i]==NULL || strlen(data[i])==0)
            continue;

        XDEBUG(printf("  [%d] %s: \"%s\"\n",i,XmGetAtomName(XtDisplay(_widget),targets[i]),data[i]));

        if (targets[i]==_XA_FILE_NAME) {
            dropInfo[numDropInfo++]=XP_STRDUP(data[i]);
        }
        if (targets[i]==_XA_NETSCAPE_URL) {
            XFE_URLDesktopType urlData(data[i]);
            for (int j=0;j<urlData.numItems();j++) {
                dropInfo[numDropInfo++]=XP_STRDUP(urlData.url(j));
            }
        }
        if (targets[i]==XA_STRING) {
            dropInfo[numDropInfo++]=XP_STRDUP(data[i]);
        }
    }
    
    int dropStatus=_attachPanel->dropAttachments((const char **)dropInfo,numDropInfo);

    // free drop info
    for(i=0;i<numDropInfo; i++)
        if (dropInfo[i])
            XP_FREE(dropInfo[i]);
    delete dropInfo;
    
    return dropStatus;
}

#if !defined(USE_MOTIF_DND)
//
// callback methods
//


void XFE_ComposeAttachFolderView::processBookmarkDrop(fe_dnd_Source *source)
{
    XFE_BookmarkView* bookmarkView=( XFE_BookmarkView*)source->closure;
    if (!bookmarkView)
        return;

    MWContext *context=bookmarkView->getContext();
    XFE_Outliner *outliner=bookmarkView->getOutliner();
    const int *selectedList;
    int numSelected;

    if (outliner->getSelection(&selectedList, &numSelected)) {
        char **items=new char*[numSelected];
        int numItems=0;
        int i;
        
        for (i=0; i<numSelected; i++) {
            BM_Entry* entry=BM_AtIndex(context,selectedList[i]+1);
            if (BM_GetType(entry)==BM_TYPE_URL ||
                BM_GetType(entry)==BM_TYPE_ALIAS) {
                const char *address=BM_GetAddress(entry);
                if (address) {
                    XDEBUG(printf("    %d:%s\n",selectedList[i],address));
                    items[numItems++]=XP_STRDUP(address);
                }
            }
        } 
        if (numItems>0)
            addAttachments((const char **) items,numItems);
        
        for (i=0; i<numItems; i++)
            XP_FREE(items[i]);
        delete items;
    }
}

void XFE_ComposeAttachFolderView::processHistoryDrop(fe_dnd_Source *source)
{
    XFE_HistoryView *historyView = (XFE_HistoryView*)source->closure;
    if (!historyView)
        return;

    MWContext *context=historyView->getContext();
    XFE_Outliner *outliner=historyView->getOutliner();
    const int *selectedList;
    int numSelected;

    if (outliner->getSelection(&selectedList, &numSelected)) {
        char **items=new char*[numSelected];
        int numItems=0;
        int i;
        
        for (i=0; i<numSelected; i++) {
            gh_HistEntry *entry=historyView->getEntry(selectedList[i]);
            if (entry && entry->address) {
                XDEBUG(printf("    %d:%s\n",selectedList[i],entry->address));
                items[numItems++]=XP_STRDUP(entry->address);
            }
        } 
        if (numItems>0)
            addAttachments((const char **) items,numItems);
        
        for (i=0; i<numItems; i++)
            XP_FREE(items[i]);
        delete items;
    }
}

void XFE_ComposeAttachFolderView::processMessageDrop(fe_dnd_Source *source)
{
    XFE_ThreadView *threadView=(XFE_ThreadView*)source->closure;
    XFE_Outliner *outliner=threadView->getOutliner();
    const int *selectedList;
    int numSelected;
    if (outliner->getSelection(&selectedList, &numSelected)) {
        char **items=new char*[numSelected];
        int numItems=0;
        int i;
        
        for (i=0; i<numSelected; i++) {
            MessageKey key=MSG_GetMessageKey(threadView->getPane(),selectedList[i]);
            URL_Struct *messageURL=MSG_ConstructUrlForMessage(threadView->getPane(),key);
            if (messageURL && messageURL->address) {
                XDEBUG(printf("    %d:%s\n",selectedList[i],messageURL->address));
                items[numItems++]=XP_STRDUP(messageURL->address);
            }
            if (messageURL)
                NET_FreeURLStruct(messageURL);
        }
        
        if (numItems>0)
            addAttachments((const char **) items,numItems);
        
        for (i=0; i<numItems; i++)
            XP_FREE(items[i]);
        delete items;
    }
}


// internal drop management
void XFE_ComposeAttachFolderView::attachDropCb(fe_dnd_Source *source)
{
    XDEBUG(printf("XFE_ComposeAttachFolderView::attachDropCb()\n"));
    
    switch (source->type) {

    // Interesting

    case FE_DND_MAIL_MESSAGE:
    case FE_DND_NEWS_MESSAGE:
        processMessageDrop(source);
        break;
    case FE_DND_BOOKMARK:
        processBookmarkDrop(source);
        break;
    case FE_DND_HISTORY:
        processHistoryDrop(source);
        break;

    // maybe interesting

    case FE_DND_ADDRESSBOOK:
        XDEBUG(printf("  AddressBook\n"));
        break;
    case FE_DND_URL:
        XDEBUG(printf("  URL\n"));
        break;

    // Not interesting

    case FE_DND_MAIL_FOLDER:
        XDEBUG(printf("  Mail Folder\n"));
        break;
    case FE_DND_NEWS_FOLDER:
        XDEBUG(printf("  News Folder\n"));
        break;
    case FE_DND_NONE:
        XDEBUG(printf("  None\n"));
        break;
    case FE_DND_COLUMN:
        XDEBUG(printf("  Column\n"));
        break;
    default:
        XDEBUG(printf("  unknown type:%d\n",source->type));
        break;
    }
}


#endif /* !USE_MOTIF_DND */
