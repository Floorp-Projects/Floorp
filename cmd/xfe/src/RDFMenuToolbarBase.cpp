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
//////////////////////////////////////////////////////////////////////////
//
// Name:        RDFMenuToolbarBase.cpp
// Description: XFE_RDFMenuToolbarBase class source.
//              Base class for components dealing with rdf toolbars
//              and menus.
// Author:      Ramiro Estrugo <ramiro@netscape.com>
// Date:        Tue Mar  4 03:45:16 PST 1997
//
//////////////////////////////////////////////////////////////////////////

#include "RDFMenuToolbarBase.h"
#include "MozillaApp.h"
#include "BrowserFrame.h"  /* for fe_reuseBrowser() */
#include "IconGroup.h"
#include "View.h"
#include "ToolbarDrop.h"

#include "felocale.h"
#include "intl_csi.h"
#include "prefapi.h"

#include <Xfe/BmButton.h>
#include <Xfe/BmCascade.h>

#include <Xfe/Cascade.h>

#include <Xm/CascadeB.h>
#include <Xm/PushB.h>
#include <Xm/Separator.h>

#define MORE_BUTTON_NAME            "bookmarkMoreButton"
#define MORE_PULLDOWN_NAME          "bookmarkPulldown"
#define MENU_PANE_SEPARATOR_NAME    "menuPaneSeparator"
#define TOOL_BAR_SEPARATOR_NAME     "toolBarSeparator"

#if DEBUG_slamm
#define D(x) x
#else
#define D(x)
#endif

#define CASCADE_WC(fancy) \
( (fancy) ? xfeBmCascadeWidgetClass : xmCascadeButtonWidgetClass )

//////////////////////////////////////////////////////////////////////////
XFE_RDFMenuToolbarBase::XFE_RDFMenuToolbarBase( XFE_Frame *  frame,
                          XP_Bool        onlyHeaders /*= FALSE*/,
                          XP_Bool        fancyItems /*= FALSE*/) :
    _frame(frame),
    _onlyHeaders(onlyHeaders),
    _fancyItems(fancyItems),
    _dropAddressBuffer(NULL),
    _dropTitleBuffer(NULL),
    _dropLastAccess(0)
{
    XP_ASSERT( _frame != NULL );

    XFE_MozillaApp::theApp()->registerInterest(
        XFE_MozillaApp::updateToolbarAppearance,
        this,
        (XFE_FunctionNotification)updateIconAppearance_cb);

    createPixmaps();
}
//////////////////////////////////////////////////////////////////////////
/* virtual */
XFE_RDFMenuToolbarBase::~XFE_RDFMenuToolbarBase()
{
    XFE_MozillaApp::theApp()->unregisterInterest(
        XFE_MozillaApp::updateToolbarAppearance,
        this,
        (XFE_FunctionNotification)updateIconAppearance_cb);

    if (_dropAddressBuffer)
    {
        XtFree(_dropAddressBuffer);
    }

    if (_dropTitleBuffer)
    {
        XtFree(_dropTitleBuffer);
    }
}
//////////////////////////////////////////////////////////////////////////

Pixmap XFE_RDFMenuToolbarBase::_bookmarkPixmap = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_RDFMenuToolbarBase::_bookmarkMask = XmUNSPECIFIED_PIXMAP;

Pixmap XFE_RDFMenuToolbarBase::_mailBookmarkPixmap = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_RDFMenuToolbarBase::_mailBookmarkMask = XmUNSPECIFIED_PIXMAP;

Pixmap XFE_RDFMenuToolbarBase::_newsBookmarkPixmap = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_RDFMenuToolbarBase::_newsBookmarkMask = XmUNSPECIFIED_PIXMAP;

Pixmap XFE_RDFMenuToolbarBase::_folderArmedPixmap = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_RDFMenuToolbarBase::_folderArmedMask = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_RDFMenuToolbarBase::_folderPixmap = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_RDFMenuToolbarBase::_folderMask = XmUNSPECIFIED_PIXMAP;

Pixmap XFE_RDFMenuToolbarBase::_newFolderArmedMask = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_RDFMenuToolbarBase::_newFolderArmedPixmap = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_RDFMenuToolbarBase::_newFolderMask = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_RDFMenuToolbarBase::_newFolderPixmap = XmUNSPECIFIED_PIXMAP;

Pixmap XFE_RDFMenuToolbarBase::_menuFolderArmedMask = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_RDFMenuToolbarBase::_menuFolderArmedPixmap = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_RDFMenuToolbarBase::_menuFolderMask = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_RDFMenuToolbarBase::_menuFolderPixmap = XmUNSPECIFIED_PIXMAP;

Pixmap XFE_RDFMenuToolbarBase::_newMenuFolderArmedMask = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_RDFMenuToolbarBase::_newMenuFolderArmedPixmap = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_RDFMenuToolbarBase::_newMenuFolderMask = XmUNSPECIFIED_PIXMAP;
Pixmap XFE_RDFMenuToolbarBase::_newMenuFolderPixmap = XmUNSPECIFIED_PIXMAP;

//////////////////////////////////////////////////////////////////////////
void
XFE_RDFMenuToolbarBase::createPixmaps()
{
    // Create icons
     Pixel fg = XfeForeground(_frame->getBaseWidget());
     Pixel bg = XfeBackground(_frame->getBaseWidget());

    if (!XfePixmapGood(XFE_RDFMenuToolbarBase::_bookmarkPixmap))
    {
        IconGroup_createAllIcons(&BM_Bookmark_group,
                                 _frame->getBaseWidget(),
                                 fg,bg);

        _bookmarkPixmap = BM_Bookmark_group.pixmap_icon.pixmap;
        _bookmarkMask = BM_Bookmark_group.pixmap_icon.mask;
    }

    if (!XfePixmapGood(XFE_RDFMenuToolbarBase::_mailBookmarkPixmap))
    {
        IconGroup_createAllIcons(&BM_MailBookmark_group,
                                 _frame->getBaseWidget(),
                                 fg,bg);

        _mailBookmarkPixmap = BM_MailBookmark_group.pixmap_icon.pixmap;
        _mailBookmarkMask = BM_MailBookmark_group.pixmap_icon.mask;
    }

    if (!XfePixmapGood(XFE_RDFMenuToolbarBase::_newsBookmarkPixmap))
    {
        IconGroup_createAllIcons(&BM_NewsBookmark_group,
                                 _frame->getBaseWidget(),
                                 fg,bg);

        _newsBookmarkPixmap = BM_NewsBookmark_group.pixmap_icon.pixmap;
        _newsBookmarkMask = BM_NewsBookmark_group.pixmap_icon.mask;
    }

    if (!XfePixmapGood(XFE_RDFMenuToolbarBase::_folderPixmap))
    {
        IconGroup_createAllIcons(&BM_Folder_group,
                                 _frame->getBaseWidget(),
                                 fg,bg);

        _folderPixmap = BM_Folder_group.pixmap_icon.pixmap;
        _folderMask = BM_Folder_group.pixmap_icon.mask;
    }

    if (!XfePixmapGood(XFE_RDFMenuToolbarBase::_folderArmedPixmap))
    {
        IconGroup_createAllIcons(&BM_FolderO_group,
                                 _frame->getBaseWidget(),
                                 fg,bg);

        _folderArmedPixmap = BM_FolderO_group.pixmap_icon.pixmap;
        _folderArmedMask = BM_FolderO_group.pixmap_icon.mask;
    }

    if (!XfePixmapGood(XFE_RDFMenuToolbarBase::_newFolderPixmap))
    {
        IconGroup_createAllIcons(&BM_NewFolder_group,
                                 _frame->getBaseWidget(),
                                 fg,bg);

        _newFolderPixmap = BM_NewFolder_group.pixmap_icon.pixmap;
        _newFolderMask = BM_NewFolder_group.pixmap_icon.mask;
    }

    if (!XfePixmapGood(XFE_RDFMenuToolbarBase::_newFolderArmedPixmap))
    {
        IconGroup_createAllIcons(&BM_NewFolderO_group,
                                 _frame->getBaseWidget(),
                                 fg,bg);

        _newFolderArmedPixmap = BM_NewFolderO_group.pixmap_icon.pixmap;
        _newFolderArmedMask = BM_NewFolderO_group.pixmap_icon.mask;
    }

    if (!XfePixmapGood(XFE_RDFMenuToolbarBase::_menuFolderPixmap))
    {
        IconGroup_createAllIcons(&BM_MenuFolder_group,
                                 _frame->getBaseWidget(),
                                 fg,bg);

        _menuFolderPixmap = BM_MenuFolder_group.pixmap_icon.pixmap;
        _menuFolderMask = BM_MenuFolder_group.pixmap_icon.mask;
    }

    if (!XfePixmapGood(XFE_RDFMenuToolbarBase::_menuFolderArmedPixmap))
    {
        IconGroup_createAllIcons(&BM_MenuFolderO_group,
                                 _frame->getBaseWidget(),
                                 fg,bg);

        _menuFolderArmedPixmap = BM_MenuFolderO_group.pixmap_icon.pixmap;
        _menuFolderArmedMask = BM_MenuFolderO_group.pixmap_icon.mask;
    }

    if (!XfePixmapGood(XFE_RDFMenuToolbarBase::_newMenuFolderPixmap))
    {
        IconGroup_createAllIcons(&BM_NewAndMenuFolder_group,
                                 _frame->getBaseWidget(),
                                 fg,bg);

        _newMenuFolderPixmap = BM_NewAndMenuFolder_group.pixmap_icon.pixmap;
        _newMenuFolderMask = BM_NewAndMenuFolder_group.pixmap_icon.mask;
    }

    if (!XfePixmapGood(XFE_RDFMenuToolbarBase::_newMenuFolderArmedPixmap))
    {
        IconGroup_createAllIcons(&BM_NewAndMenuFolderO_group,
                                 _frame->getBaseWidget(),
                                 fg,bg);

        _newMenuFolderArmedPixmap = BM_NewAndMenuFolderO_group.pixmap_icon.pixmap;
        _newMenuFolderArmedMask = BM_NewAndMenuFolderO_group.pixmap_icon.mask;
    }
}
//////////////////////////////////////////////////////////////////////////

/* static */ void
XFE_RDFMenuToolbarBase::item_armed_cb(Widget       w,
                                      XtPointer    clientData,
                                      XtPointer    /*call_data*/)
{
    ItemCallbackStruct *   data = (ItemCallbackStruct *) clientData;
    XFE_RDFMenuToolbarBase *   object = data->object;
    HT_Resource                entry = data->entry;

    object->entryArmed(w,entry);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_RDFMenuToolbarBase::item_enter_cb(Widget       w,
                                      XtPointer    clientData,
                                      XtPointer    /* call_data */)
{
    ItemCallbackStruct *    data = (ItemCallbackStruct *) clientData;
    XFE_RDFMenuToolbarBase *            object = data->object;
    HT_Resource                    entry = data->entry;

    object->entryEnter(w,entry);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_RDFMenuToolbarBase::item_leave_cb(Widget       w,
                                      XtPointer    clientData,
                                      XtPointer    /* call_data */)
{
    ItemCallbackStruct *    data = (ItemCallbackStruct *) clientData;
    XFE_RDFMenuToolbarBase *            object = data->object;
    HT_Resource                    entry = data->entry;

    object->entryLeave(w,entry);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_RDFMenuToolbarBase::item_disarmed_cb(Widget       w,
                                         XtPointer    clientData,
                                         XtPointer    /* call_data */)
{
    ItemCallbackStruct *    data = (ItemCallbackStruct *) clientData;
    XFE_RDFMenuToolbarBase *            object = data->object;
    HT_Resource                    entry = data->entry;

    object->entryDisarmed(w,entry);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_RDFMenuToolbarBase::item_activated_cb(Widget       w,
                                          XtPointer    clientData,
                                          XtPointer    /* call_data */)
{
    ItemCallbackStruct *    data = (ItemCallbackStruct *) clientData;
    XFE_RDFMenuToolbarBase *            object = data->object;
    HT_Resource                    entry = data->entry;

    if (XfeBmAccentIsEnabled() && (XfeIsBmButton(w) || XfeIsBmCascade(w)))
    {
        unsigned char accent_type;
        
        XtVaGetValues(w,XmNaccentType,&accent_type,NULL);

        // Add the new entry above the activated bookmark item
        if (accent_type == XmACCENT_TOP)
        {
            object->addEntryBefore(object->getDropAddress(),
                                   object->getDropTitle(),
                                   object->getDropLastAccess(),
                                   entry);
        }
        // Add the new entry below the activated bookmark item
        else if (accent_type == XmACCENT_BOTTOM)
        {
            object->addEntryAfter(object->getDropAddress(),
                                  object->getDropTitle(),
                                  object->getDropLastAccess(),
                                  entry);
        }
        // Add the new entry into the activated bookmark folder
        else if (accent_type == XmACCENT_ALL)
        {
            object->addEntryToFolder(object->getDropAddress(),
                                     object->getDropTitle(),
                                     object->getDropLastAccess(),
                                     entry);
        }
        
        // Now disable dropping
        object->disableDropping();
    }
    // A fancy cascade button was activated
    else if (XmIsCascadeButton(w) && entry && HT_IsContainer(entry))
    {
        XFE_Frame *        frame = object->getFrame();
        MWContext *        context = frame->getContext();
        History_entry * he = SHIST_GetCurrent(&context->hist);

        char *            address;
        char *            title;
        time_t            lastAccess;

        XP_ASSERT( frame != NULL );
        XP_ASSERT( context != NULL );
        XP_ASSERT( he != NULL );

        address = he->address;
        
        title = (he->title && *he->title) ? he->title : address;

        lastAccess = he->last_access;

        object->addEntryToFolder(address,title,lastAccess,entry);
    }
    // A push button was activated
    else
    {
        object->entryActivated(w,entry);
    }
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_RDFMenuToolbarBase::item_cascading_cb(Widget        cascade,
                                          XtPointer     clientData,
                                          XtPointer     /* call_data */)
{
    ItemCallbackStruct *    data = (ItemCallbackStruct *) clientData;
    XFE_RDFMenuToolbarBase *            object = data->object;
    HT_Resource                    entry = data->entry;
    Widget                        submenu = NULL;

    XtVaGetValues(cascade,XmNsubMenuId,&submenu,NULL);

    if (entry && submenu) 
    {
        XP_ASSERT(HT_IsContainer(entry));

        // Add the first if in a "File Bookmark" hierarchy.  This should
        // only happen if _onlyHeaders is true and the current bookmark
        // header has at least one item that is also a header.
        if (object->_onlyHeaders)
        {
            Widget file_here = object->createCascadeButton(submenu,entry,True);
            Widget sep = object->createSeparator(submenu);

            XtManageChild(file_here);
            XtManageChild(sep);
        }

        object->createItemTree(submenu, entry);
    }

    XtRemoveCallback(cascade,
                     XmNcascadingCallback,
                     &XFE_RDFMenuToolbarBase::item_cascading_cb,
                     (XtPointer) data);

    object->entryCascading(cascade,entry);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_RDFMenuToolbarBase::item_free_data_cb(Widget       /* item */,
                                          XtPointer    clientData,
                                          XtPointer    /* call_data */)
{
    ItemCallbackStruct *    data = (ItemCallbackStruct *) clientData;

    XP_FREE(data);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_RDFMenuToolbarBase::pane_mapping_eh(Widget        submenu,
                                        XtPointer     clientData,
                                        XEvent *      event,
                                        Boolean *     cont)
{
     XFE_RDFMenuToolbarBase * object = (XFE_RDFMenuToolbarBase *) clientData;

    // Make sure the widget is alive and this is an unmap event
    if (!XfeIsAlive(submenu) || !event || (event->type != UnmapNotify))
    {
        return;
    }

    // Look for the cascade ancestor
    Widget cascade = 
        XfeAncestorFindByClass(submenu,xfeCascadeWidgetClass,XfeFIND_ANY);
               
    if (cascade)
    {
        Boolean armed;

        // Determine if the cascade is armed
        XtVaGetValues(cascade,XmNarmed,&armed,NULL);

        // If it is armed, then it means the menu hierarchy is still 
        // visible and a shared submenu as the one that caused this 
        // event.  Those motif shared sumenu shells are very complicated.

        if (armed)
        {
            return;
        }

        object->disableDropping();
    }

    *cont = True;
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_RDFMenuToolbarBase,updateIconAppearance)
    (XFE_NotificationCenter *    /*obj*/, 
     void *                        /*clientData*/, 
     void *                        /*callData*/)
{
    // Update the appearance
    updateAppearance();
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFMenuToolbarBase::getPixmapsForEntry(HT_Resource    entry,
                                     Pixmap *    pixmapOut,
                                     Pixmap *    maskOut,
                                     Pixmap *    armedPixmapOut,
                                     Pixmap *    armedMaskOut)
{
    Pixmap pixmap          = _bookmarkPixmap;
    Pixmap mask            = _bookmarkMask;
    Pixmap armedPixmap     = XmUNSPECIFIED_PIXMAP;
    Pixmap armedMask       = XmUNSPECIFIED_PIXMAP;

    // Right now the only way an entry can be NULL is for the 
    // bookmarkMoreButton which kinda looks and acts like a folder so
    // we use folder pixmaps for it
    if (!entry)
    {
        pixmap         = _folderPixmap;
        mask           = _folderMask;
        armedPixmap    = _folderArmedPixmap;
        armedMask      = _folderArmedMask;
    }
    else
    {
#ifdef NOT_YET
        if (hasCustomIcon(entry)
        {
        } else
#endif /*NOT_YET*/
        if (ht_IsFECommand(entry))
        {
            const char* url = HT_GetNodeURL(entry);
            
            IconGroup *ig = IconGroup_findGroupForName(url + 8);
            if (ig)
            {
                pixmap        = ig->pixmap_icon.pixmap;
                mask          = ig->pixmap_icon.mask;
                armedPixmap   = ig->pixmap_mo_icon.pixmap;
                armedMask     = ig->pixmap_mo_icon.mask;
            }
        } 
        else
        {
            if (HT_IsContainer(entry))
            {
                XP_Bool is_menu = (entry == getMenuFolder());
                XP_Bool is_add  = (entry == getAddFolder());
            
                if (is_add && is_menu)
                {
                    pixmap         = _newMenuFolderPixmap;
                    mask           = _newMenuFolderMask;
                    armedPixmap    = _newMenuFolderArmedPixmap;
                    armedMask      = _newMenuFolderArmedMask;
                }
                else if (is_add)
                {
                    pixmap         = _newFolderPixmap;
                    mask           = _newFolderMask;
                    armedPixmap    = _newFolderArmedPixmap;
                    armedMask      = _newFolderArmedMask;
                }
                else if (is_menu)
                {
                    pixmap         = _menuFolderPixmap;
                    mask           = _menuFolderMask;
                    armedPixmap    = _menuFolderArmedPixmap;
                    armedMask      = _menuFolderArmedMask;
                }
                else
                {
                    pixmap         = _folderPixmap;
                    mask           = _folderMask;
                    armedPixmap    = _folderArmedPixmap;
                    armedMask      = _folderArmedMask;
                }
            }
            else
            {
                int url_type = NET_URL_Type(HT_GetNodeURL(entry));
                
                switch (url_type)
                {
                case IMAP_TYPE_URL:
                case MAILBOX_TYPE_URL:
                    
                    pixmap    = _mailBookmarkPixmap;
                    mask      = _mailBookmarkMask;
                    
                    break;
                    
                case NEWS_TYPE_URL:
                    
                    pixmap    = _newsBookmarkPixmap;
                    mask      = _newsBookmarkMask;
                    
                    break;
                    
                default:
                    
                    pixmap    = _bookmarkPixmap;
                    mask      = _bookmarkMask;
                    
                    break;
                }
            }
        }
    }

    if (pixmapOut)
    {
        *pixmapOut = pixmap;
    }

    if (maskOut)
    {
        *maskOut = mask;
    }

    if (armedPixmapOut)
    {
        *armedPixmapOut = armedPixmap;
    }

    if (armedMaskOut)
    {
        *armedMaskOut = armedMask;
    }

}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_RDFMenuToolbarBase::getBookmarkPixmaps(Pixmap & pixmap_out,Pixmap & mask_out)
{
    pixmap_out = _bookmarkPixmap;
    mask_out = _bookmarkMask;
}
#if 0
//////////////////////////////////////////////////////////////////////////
HT_Resource
XFE_RDFMenuToolbarBase::getFirstEntry()
{
    // Access the root header
    HT_Resource root = getMenuForder();

    // Ignore the root header (ie, "Joe's Bookmarks")
    if (root && HT_IsContainer(root))
    {
        root = HT_GetChildren(root);
    }

    return root;
}
//////////////////////////////////////////////////////////////////////////
HT_Resource
XFE_RDFMenuToolbarBase::getTopLevelFolder(const char * name)
{
    HT_Resource result = NULL;

    // Access the root header
    HT_Resource root = getRootFolder();

    // Ignore the root header (ie, "Joe's Bookmarks")
    if (root && HT_IsContainer(root))
    {
        HT_Resource entry = HT_GetChildren(root);

        while (entry && !result)
        {
            if (HT_IsContainer(entry))
            {
                if (XP_STRCMP(name,HT_GetName(entry)) == 0)
                {
                    result = entry;
                }
            }

            entry = HT_GetNext(entry);
        }
    }

    return result;
}
#endif /*0*/
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFMenuToolbarBase::trackSubmenuMapping(Widget submenu)
{
    XP_ASSERT( XfeIsAlive(submenu) );
    XP_ASSERT( XmIsRowColumn(submenu) );

    Widget shell = XtParent(submenu);

    XP_ASSERT( shell );

    XtAddEventHandler(shell,
                      StructureNotifyMask,
                      True,
                      &XFE_RDFMenuToolbarBase::pane_mapping_eh,
                      (XtPointer) this);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFMenuToolbarBase::createItemTree(Widget menu,HT_Resource entry)
{
    XP_ASSERT( HT_IsContainer(entry) );

    D(printf("Create tree: %s\n",HT_GetNodeName(entry)));
    HT_SetOpenState(entry, PR_TRUE);
    HT_Cursor child_cursor = HT_NewCursor(entry);
    HT_Resource child;
    while ( (child = HT_GetNextItem(child_cursor)) )
    {
        Widget item = NULL;

        if (HT_IsContainer(child))
        {
            item = createCascadeButton(menu,child,False);
        }
        else if (!_onlyHeaders)
        {
            if (HT_IsSeparator(child))
            {
                item = createSeparator(menu);
            }
            else
            {
                item = createPushButton(menu,child);
            }
        }

        if (item)
        {
            XP_ASSERT( XfeIsAlive(item) );

            XtManageChild(item);
        }
    }
    HT_DeleteCursor(child_cursor);
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFMenuToolbarBase::prepareToUpdateRoot()
{
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFMenuToolbarBase::updateAppearance()
{
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFMenuToolbarBase::updateToolbarFolderName()
{
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFMenuToolbarBase::entryArmed(Widget /* button */,HT_Resource entry)
{
    char * address;

    if (!entry || HT_IsContainer(entry) || HT_IsSeparator(entry))
    {
        return;
    }
    
    address = HT_GetNodeURL(entry);
    
    _frame->notifyInterested(XFE_View::statusNeedsUpdatingMidTruncated,
                             (void*) address);
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFMenuToolbarBase::entryEnter(Widget button,HT_Resource entry)
{
    // Same thing as armed
    entryArmed(button,entry);
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFMenuToolbarBase::entryLeave(Widget button,HT_Resource entry)
{
    // Same thing as disarmed
    entryDisarmed(button,entry);
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFMenuToolbarBase::entryDisarmed(Widget /* button */,HT_Resource entry)
{
    if (entry && !HT_IsContainer(entry) && !HT_IsSeparator(entry))
    {
        _frame->notifyInterested(XFE_View::statusNeedsUpdatingMidTruncated,
                                 (void*) "");
    }
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFMenuToolbarBase::entryActivated(Widget w, HT_Resource entry)
{
    if (entry)
    {
        if (ht_IsFECommand(entry)) 
        {
            Cardinal numparams = 1;
            CommandType cmd = ht_GetFECommand(entry);

            XFE_CommandInfo info(XFE_COMMAND_EVENT_ACTION,
                                 w, 
                                 NULL /*event*/,
                                 NULL /*params*/,
                                 0    /*num_params*/);
            if (_frame->handlesCommand(cmd, NULL /*calldata*/, &info)
                && _frame->isCommandEnabled(cmd, NULL /*calldata*/, &info))
            {
                xfe_ExecuteCommand(_frame, cmd, NULL /*calldata*/, &info);
		
                //_frame->notifyInterested(Command::commandDispatchedCallback, 
                //callData);
            }

        }
        else if (!HT_IsContainer(entry) && !HT_IsSeparator(entry))
        {
            char *s = HT_GetNodeURL(entry);
            URL_Struct *url = NET_CreateURLStruct (s, NET_DONT_RELOAD);
            fe_reuseBrowser (_frame->getContext(), url);
        }
    }
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFMenuToolbarBase::entryCascading(Widget /* cascade */,HT_Resource /* entry */)
{
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFMenuToolbarBase::enableDropping()
{
    // Enable the drawing of accents on the menu hierarchy
    XfeBmAccentSetFileMode(XmACCENT_FILE_ANYWHERE);
    XfeBmAccentEnable();
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFMenuToolbarBase::disableDropping()
{
    // Disable the drawing of accents on the menu hierarchy
    XfeBmAccentDisable();
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ XP_Bool
XFE_RDFMenuToolbarBase::isDroppingEnabled()
{
    return XfeBmAccentIsEnabled();
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Menu component creation functions
//
//////////////////////////////////////////////////////////////////////////
Widget 
XFE_RDFMenuToolbarBase::createCascadeButton(Widget        menu,
                                            HT_Resource   entry,
                                            XP_Bool       ignore_children)
{
    XP_ASSERT( XfeIsAlive(menu) );
    XP_ASSERT( XmIsRowColumn(menu) );

    D(printf("Create cascade: %s\n",HT_GetNodeName(entry)));

    Widget                  cascade = NULL;
    Widget                  pulldown = NULL;

    Widget                  non_full_menu;

    String                  pulldown_name = NULL;

    ItemCallbackStruct *    data = NULL;

    non_full_menu = getLastMoreMenu(menu);

    XP_ASSERT( XfeIsAlive(non_full_menu) );

    //    
    // Determine if we need a sub menu
    //

    // Create some extra booleans to clear up the logic
    // of the if-statement below.
    XP_Bool want_children                = !ignore_children;
    XP_Bool want_non_folder_children_too = !_onlyHeaders;
    
    if (want_children && 
        (want_non_folder_children_too
         || ht_FolderHasFolderChildren(entry)))
    {
        pulldown_name = "bookmarkPulldown";
    }

    //
    // Create the item
    //
    XfeMenuCreatePulldownPane(non_full_menu,
                              non_full_menu,
                              "bookmarkCascadeButton",
                              pulldown_name,
                              CASCADE_WC(_fancyItems),
                              False,
                              NULL,
                              0,
                              &cascade,
                              &pulldown);

    // Set the item's label
    setItemLabelString(cascade,entry);
    
    // Configure the new cascade button
    if (_fancyItems)
    {
        configureXfeBmCascade(cascade,entry);
    }
    else
    {
        configureCascade(cascade,entry);
    }

    // Create a new bookmark data structure for the callbacks
    data = XP_NEW_ZAP(ItemCallbackStruct);

    data->object    = this;
    data->entry        = entry;

    XtAddCallback(cascade,
                  XmNcascadingCallback,
                  &XFE_RDFMenuToolbarBase::item_cascading_cb,
                  (XtPointer) data);

    XtAddCallback(cascade,
                  XmNactivateCallback,
                  &XFE_RDFMenuToolbarBase::item_activated_cb,
                  (XtPointer) data);

    XtAddCallback(cascade,
                  XmNdestroyCallback,
                  &XFE_RDFMenuToolbarBase::item_free_data_cb,
                  (XtPointer) data);

    return cascade;
}
//////////////////////////////////////////////////////////////////////////
Widget 
XFE_RDFMenuToolbarBase::createMoreButton(Widget menu)
{
    Widget   cascade = NULL;
    Widget   pulldown = NULL;

    // Create a pulldown pane (cascade + pulldown)
    XfeMenuCreatePulldownPane(menu,
                              menu,
                              MORE_BUTTON_NAME,
                              MORE_PULLDOWN_NAME,
                              CASCADE_WC(_fancyItems),
                              False,
                              NULL,
                              0,
                              &cascade,
                              &pulldown);

    if (_fancyItems)
    {
        configureXfeBmCascade(cascade,NULL);
    }
    else
    {
        configureCascade(cascade,NULL);
    }

    return cascade;
}
//////////////////////////////////////////////////////////////////////////
Widget 
XFE_RDFMenuToolbarBase::createPushButton(Widget menu, HT_Resource entry)
{
    XP_ASSERT( XfeIsAlive(menu) );
    XP_ASSERT( XmIsRowColumn(menu) );

    D(printf("Create push : %s\n",HT_GetNodeName(entry)));

    Widget                        non_full_menu;
    Widget                        button;
    ItemCallbackStruct *    data = NULL;

    non_full_menu = getLastMoreMenu(menu);

    XP_ASSERT( XfeIsAlive(non_full_menu) );

    // must be xfeCmdOpenTargetUrl or sensitivity becomes a problem
    if (_fancyItems)
    {
        button = XfeCreateBmButton(non_full_menu,xfeCmdOpenTargetUrl,NULL,0);

        configureXfeBmButton(button,entry);
    }
    else
    {
        button = XmCreatePushButton(non_full_menu,xfeCmdOpenTargetUrl,NULL,0);

        configureButton(button,entry);
    }

    // Set the item's label
    setItemLabelString(button,entry);

    // Create a new bookmark data structure for the callbacks
    data = XP_NEW_ZAP(ItemCallbackStruct);

    data->object    = this;
    data->entry        = entry;
    
    XtAddCallback(button,
                  XmNactivateCallback,
                  &XFE_RDFMenuToolbarBase::item_activated_cb,
                  (XtPointer) data);

    XtAddCallback(button,
                  XmNarmCallback,
                  &XFE_RDFMenuToolbarBase::item_armed_cb,
                  (XtPointer) data);

    XtAddCallback(button,
                  XmNdisarmCallback,
                  &XFE_RDFMenuToolbarBase::item_disarmed_cb,
                  (XtPointer) data);

    XtAddCallback(button,
                  XmNdestroyCallback,
                  &XFE_RDFMenuToolbarBase::item_free_data_cb,
                  (XtPointer) data);

    return button;
}
//////////////////////////////////////////////////////////////////////////
Widget 
XFE_RDFMenuToolbarBase::getLastMoreMenu(Widget menu)
{
    XP_ASSERT( XfeIsAlive(menu) );
    XP_ASSERT( XmIsRowColumn(menu) );

    // Find the last more... menu
    Widget last_more_menu = XfeMenuFindLastMoreMenu(menu,MORE_BUTTON_NAME);

    XP_ASSERT( XfeIsAlive(last_more_menu) );

    // Check if the last menu is full
    if (XfeMenuIsFull(last_more_menu))
    {
        // Look for the More... button for the last menu
        Widget more_button = XfeMenuGetMoreButton(last_more_menu,
                                                  MORE_BUTTON_NAME);

        // If no more button, create one plus a submenu
        if (!more_button)
        {
            more_button = createMoreButton(last_more_menu);

            XtManageChild(more_button);

        }

        // Set the last more menu to the submenu of the new more button
        last_more_menu = XfeCascadeGetSubMenu(more_button);
    }

    return last_more_menu;
}
//////////////////////////////////////////////////////////////////////////
Widget 
XFE_RDFMenuToolbarBase::createSeparator(Widget menu)
{
    XP_ASSERT( XfeIsAlive(menu) );

    Widget     parent;        
    Widget     separator;
    String     name;

    if (XmIsRowColumn(menu))
    {
        parent = getLastMoreMenu(menu);

        name = MENU_PANE_SEPARATOR_NAME;
    }
    else
    {
        parent = menu;

        name = TOOL_BAR_SEPARATOR_NAME;
    }

    XP_ASSERT( XfeIsAlive(parent) );

    separator = XmCreateSeparator(parent,name,NULL,0);

    configureSeparator(separator,NULL);

    XtManageChild(separator);
    
    return separator;
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFMenuToolbarBase::configureXfeBmButton(Widget item,HT_Resource entry)
{
    int32 toolbar_style;
    PREF_GetIntPref("browser.chrome.toolbar_style", &toolbar_style);

    D(printf("XFE_RDFMenuToolbarBase::configureXfeCascade: toolbar_style = %d\n",
             toolbar_style););


    if (toolbar_style == BROWSER_TOOLBAR_TEXT_ONLY)
    {
        XtVaSetValues(item,
                      XmNlabelPixmap,        XmUNSPECIFIED_PIXMAP,
                      XmNlabelPixmapMask,    XmUNSPECIFIED_PIXMAP,
                      NULL);

    }
    else
    {
        Pixmap pixmap;
        Pixmap mask;

        getPixmapsForEntry(entry,&pixmap,&mask,NULL,NULL);

        XtVaSetValues(item,
                      XmNlabelPixmap,        pixmap,
                      XmNlabelPixmapMask,    mask,
                      NULL);
    }
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFMenuToolbarBase::configureXfeBmCascade(Widget item,HT_Resource entry)
{
    int32 toolbar_style;
    PREF_GetIntPref("browser.chrome.toolbar_style", &toolbar_style);

    D(printf("XFE_RDFMenuToolbarBase::configureXfeBmCascade: toolbar_style = %d\n",
             toolbar_style););
    if (toolbar_style == BROWSER_TOOLBAR_TEXT_ONLY)
    {
        XtVaSetValues(item,
                      XmNlabelPixmap,        XmUNSPECIFIED_PIXMAP,
                      XmNlabelPixmapMask,    XmUNSPECIFIED_PIXMAP,
                      XmNarmPixmap,          XmUNSPECIFIED_PIXMAP,
                      XmNarmPixmapMask,      XmUNSPECIFIED_PIXMAP,
                      NULL);
    }
    else
    {        
        Pixmap pixmap;
        Pixmap mask;
        Pixmap armedPixmap;
        Pixmap armedMask;
        
        getPixmapsForEntry(entry,&pixmap,&mask,&armedPixmap,&armedMask);
        
        Arg         av[4];
        Cardinal    ac = 0;
        
        XtSetArg(av[ac],XmNlabelPixmap,        pixmap); ac++;
        XtSetArg(av[ac],XmNlabelPixmapMask,    mask);   ac++;
        
        // Only show the aremd pixmap/mask if this entry has children
        if (XfeIsAlive(XfeCascadeGetSubMenu(item)))
        {
            XtSetArg(av[ac],XmNarmPixmap,        armedPixmap); ac++;
            XtSetArg(av[ac],XmNarmPixmapMask,    armedMask);   ac++;
        }
        
        XtSetValues(item,av,ac);
    }
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFMenuToolbarBase::configureButton(Widget      /*item*/,
                                        HT_Resource /*entry*/)
{
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFMenuToolbarBase::configureCascade(Widget      /*item*/,
                                         HT_Resource /*entry*/)
{
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFMenuToolbarBase::configureSeparator(Widget      /*item*/,
                                           HT_Resource /*entry*/)
{
}
//////////////////////////////////////////////////////////////////////////
HT_Resource
XFE_RDFMenuToolbarBase::getAddFolder()
{
    return getRootFolder();
}
//////////////////////////////////////////////////////////////////////////
HT_Resource
XFE_RDFMenuToolbarBase::getMenuFolder()
{
    //xxx This needs to get the quickfile folder from HT
    return getRootFolder();
}
//////////////////////////////////////////////////////////////////////////
XP_Bool
XFE_RDFMenuToolbarBase::getOnlyHeaders()
{
	return _onlyHeaders;
}
//////////////////////////////////////////////////////////////////////////
/* static */ HT_Resource
XFE_RDFMenuToolbarBase::ht_FindFolderByName(HT_Resource root,
                                            char * folder_name)
{
    if (!root || !folder_name)
    {
        return NULL;
    }

    HT_Resource header;

    char * header_name = HT_GetNodeName(root);

    if(XP_STRCMP(header_name,folder_name) == 0)
    {
        return root;
    }

    
    HT_SetOpenState(root, PR_TRUE);
    HT_Cursor child_cursor = HT_NewCursor(root);
    HT_Resource child;
    while ( (child = HT_GetNextItem(child_cursor)) )
    {
        if (HT_IsContainer(child))
        {
            header = ht_FindFolderByName(child, folder_name);

            if(header != NULL)
            {
                HT_DeleteCursor(child_cursor);
                return header;
            }
        }
    }
    HT_DeleteCursor(child_cursor);

    return NULL;
}
//////////////////////////////////////////////////////////////////////////
/* static */ HT_Resource
XFE_RDFMenuToolbarBase::ht_FindItemByAddress(HT_Resource     root,
                                              const char *    target_address)
{
    if (!root || !target_address)
    {
        return NULL;
    }

    char * entry_address = HT_GetNodeURL(root);

    if (entry_address && (XP_STRCMP(target_address,entry_address) == 0))
    {
        return root;
    }

    if (HT_IsContainer(root))
    {
        HT_Resource entry;

        HT_SetOpenState(entry, PR_TRUE);
        HT_Cursor child_cursor = HT_NewCursor(entry);
        HT_Resource child;
        while ( (child = HT_GetNextItem(child_cursor)) )
        {
            HT_Resource test_entry;

            test_entry = ht_FindItemByAddress(entry,target_address);
            
            if (test_entry)
            {
                HT_DeleteCursor(child_cursor);
                return test_entry;
            }
        }
        HT_DeleteCursor(child_cursor);
    }

    return NULL;
}
//////////////////////////////////////////////////////////////////////////
/* static */ HT_Resource
XFE_RDFMenuToolbarBase::ht_FindNextItem(HT_Resource current)
{
    if (current)
    {
        HT_View ht_view = HT_GetView(current);
        uint32 index = HT_GetNodeIndex(ht_view, current);
        return HT_GetNthItem(ht_view, index + 1);
     }
    
     return NULL;
}
//////////////////////////////////////////////////////////////////////////
/* static */ HT_Resource
XFE_RDFMenuToolbarBase::ht_FindPreviousItem(HT_Resource current)
{

    if (current)
    {
        HT_View ht_view = HT_GetView(current);
        uint32 index = HT_GetNodeIndex(ht_view, current);
        if (index > 0)
            return HT_GetNthItem(ht_view, index - 1);
    }

    return NULL;
}
//////////////////////////////////////////////////////////////////////////
/* static */ XP_Bool
XFE_RDFMenuToolbarBase::ht_FolderHasFolderChildren(HT_Resource header)
{
    XP_ASSERT( header != NULL );
    XP_ASSERT( HT_IsContainer(header) );

    HT_SetOpenState(header, PR_TRUE);
    HT_Cursor child_cursor = HT_NewCursor(header);
    HT_Resource child;

    // Traverse until we find a header entry
    while ( (child = HT_GetNextItem(child_cursor)) )
    {
        if (HT_IsContainer(child)) 
        {
            HT_DeleteCursor(child_cursor);
            return True;
        }
    }
    HT_DeleteCursor(child_cursor);
    return False;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFMenuToolbarBase::addEntryAfter(const char *    address,
                                const char *    title,
                                time_t            /*lastAccess*/,
                                HT_Resource        entryAfter)
{
    XP_ASSERT( entryAfter != NULL );

    char * url = XP_STRDUP(address);
    char * tit = XP_STRDUP(title);

    HT_DropURLAndTitleAtPos(entryAfter, url, tit, PR_FALSE);

    XP_FREE(url);
    XP_FREE(tit);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFMenuToolbarBase::addEntryBefore(const char *    address,
                                 const char *    title,
                                 time_t        /*lastAccess*/,
                                 HT_Resource        entryBefore)
{
    XP_ASSERT( entryBefore != NULL );

    char * url = XP_STRDUP(address);
    char * tit = XP_STRDUP(title);

    HT_DropURLAndTitleAtPos(entryBefore, url, tit, PR_TRUE);

    XP_FREE(url);
    XP_FREE(tit);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFMenuToolbarBase::addEntryToFolder(const char *        address,
                                   const char *        title,
                                   time_t            /*lastAccess*/,
                                   HT_Resource        header)
{
    XP_ASSERT( header != NULL );
    XP_ASSERT( HT_IsContainer(header) );

    char * url = XP_STRDUP(address);
    char * tit = XP_STRDUP(title);

    HT_DropURLAndTitleOn(header, url, tit);

    XP_FREE(url);
    XP_FREE(tit);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFMenuToolbarBase::setDropAddress(const char * address)
{
    if (_dropAddressBuffer)
    {
        XtFree(_dropAddressBuffer);
    }

    _dropAddressBuffer = NULL;

    if (address)
    {
        _dropAddressBuffer = (char *) XtNewString(address);
    }
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFMenuToolbarBase::setDropTitle(const char * title)
{
    if (_dropTitleBuffer)
    {
        XtFree(_dropTitleBuffer);
    }

    _dropTitleBuffer = NULL;

    if (title)
    {
        _dropTitleBuffer = (char *) XtNewString(title);
    }
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFMenuToolbarBase::setDropLastAccess(time_t lastAccess)
{
    _dropLastAccess = lastAccess;
}
//////////////////////////////////////////////////////////////////////////
const char *
XFE_RDFMenuToolbarBase::getDropAddress()
{
    return _dropAddressBuffer;
}
//////////////////////////////////////////////////////////////////////////
const char *
XFE_RDFMenuToolbarBase::getDropTitle()
{
    return _dropTitleBuffer;
}
//////////////////////////////////////////////////////////////////////////
time_t
XFE_RDFMenuToolbarBase::getDropLastAccess()
{
    return _dropLastAccess;
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_RDFMenuToolbarBase::guessTitle(XFE_Frame *    frame,
                        const char *    address,
                        XP_Bool        sameShell,
                        char **        guessedTitleOut,
                        time_t *        guessedLastDateOut)
{
    XP_ASSERT( frame != NULL );
    XP_ASSERT( address != NULL );
    XP_ASSERT( guessedTitleOut != NULL );
    XP_ASSERT( guessedLastDateOut != NULL );
    
    // Initialize the results to begin with
    *guessedLastDateOut    = 0;
    *guessedTitleOut        = NULL;

    // Obtain the frame's context
    MWContext *    frame_context = frame->getContext();

    // If the operation occurs in the same shell, look in the history.
    if (sameShell)
    {
        // Obtain the current history entry
        History_entry * hist_entry = SHIST_GetCurrent(&frame_context->hist);

        // Make sure the history entry matches the given address, or else
        // the caller used a the same shell with a link that is not the 
        // the proxy icon (ie, in the html area).
        if (hist_entry && (XP_STRCMP(hist_entry->address,address) == 0))
        {
            *guessedTitleOut = hist_entry->title;
            *guessedLastDateOut = hist_entry->last_access;
        }
    }

#if 0
/* Currently don't for a static way to get the bookmarks */

    // Look in the bookmarks for a dup
    if (!*guessedTitleOut)
    {
        HT_Resource test_entry = NULL;
        HT_Resource root_entry = getRootFolder();

        if (root_entry)
        {
            test_entry = ht_FindItemByAddress(root_entry, address);
        }

        if (test_entry)
        {
            *guessedTitleOut = HT_GetNodeName(test_entry);
        }
    }
#endif

    // As a last resort use the address itself as a title
    if (!*guessedTitleOut)
    {
        *guessedTitleOut = (char *) address;
    }

    // Strip the leading http:// from the title
    if (XP_STRLEN(*guessedTitleOut) > 7)
    {
        if ((XP_STRNCMP(*guessedTitleOut,"http://",7) == 0) ||
            (XP_STRNCMP(*guessedTitleOut,"file://",7) == 0))
        {
            (*guessedTitleOut) += 7;
        }
    }
}
//////////////////////////////////////////////////////////////////////////

/* static */ XmString
XFE_RDFMenuToolbarBase::entryToXmString(HT_Resource        entry,
                                  INTL_CharSetInfo    char_set_info)
{
    XP_ASSERT( entry != NULL );

    XmString            result = NULL;
    XmString            tmp;
    char *                psz;

    tmp = XFE_RDFMenuToolbarBase::formatItem(entry,
                                       True,
                                       INTL_DefaultWinCharSetID(NULL));
    
    // Mid truncate the name
    if (XmStringGetLtoR(tmp,XmSTRING_DEFAULT_CHARSET,&psz))
    {

        XmStringFree(tmp);

        INTL_MidTruncateString(INTL_GetCSIWinCSID(char_set_info),psz,psz,40);

        result = XmStringCreateLtoR(psz,XmSTRING_DEFAULT_CHARSET);

        if (psz)
        {
            XtFree(psz);
        }
    }
    else
    {
        result = tmp;
    }

    return result;
}
//////////////////////////////////////////////////////////////////////////
XmString
XFE_RDFMenuToolbarBase::formatItem(HT_Resource        entry, 
                             Boolean        no_indent,
                             int16            charset)
{
  XmString xmhead, xmtail, xmcombo;
  int depth = (no_indent ? 0 : HT_GetItemIndentation(entry));
  char head [255];
  char buf [1024];
  int j = 0;
  char *title = HT_GetNodeName(entry);
  char *url = HT_GetNodeURL(entry);
  int indent = 2;
  int left_offset = 2;
  XmFontList font_list;

  if (! no_indent)
    {
      head [j++] = (HT_IsContainer(entry)
                    ? (HT_IsContainerOpen(entry) ? '-' : '+') :
                    HT_IsSeparator(entry) ? ' ' :
                    /* ### item->last_visit == 0*/ 0 ? '?' : ' ');
      while (j < ((depth * indent) + left_offset))
          head [j++] = ' ';
    }
  head [j] = 0;

  if (HT_IsSeparator(entry))
    {
      strcpy (buf, "-------------------------");
    }
  else if (title || url)
    {
      fe_FormatDocTitle (title, url, buf, 1024);
    }
#if 0
  else if (HT_IsUrl(entry))
    {
      strcpy (buf, XP_GetString( XFE_GG_EMPTY_LL ) );
    }
#endif

  if (!*head)
    xmhead = 0;
  else
    xmhead = XmStringSegmentCreate (head, "ICON", XmSTRING_DIRECTION_L_TO_R,
                    False);

  if (!*buf)
    xmtail = 0;
  else if (title || url)
    xmtail = fe_ConvertToXmString ((unsigned char *) buf, charset, 
                                   NULL, XmFONT_IS_FONT, &font_list);
  else
    {
      char *loc;

      loc = (char *) fe_ConvertToLocaleEncoding (charset,
                                                 (unsigned char *) buf);
      xmtail = XmStringSegmentCreate (loc, "HEADING",
                                      XmSTRING_DIRECTION_L_TO_R, False);
      if (loc != buf)
      {
          XP_FREE(loc);
      }
    }

  if (xmhead && xmtail)
    {
      int size = XmStringLength (xmtail);

      /*
     There is a bug down under XmStringNConcat() where, in the process of
     appending the two strings, it will read up to four bytes past the end
     of the second string.  It doesn't do anything with the data it reads
     from there, but people have been regularly crashing as a result of it,
     because sometimes these strings end up at the end of the page!!

     I tried making the second string a bit longer (by adding some spaces
     to the end of `buf') and passing in a correspondingly smaller length
     to XmStringNConcat(), but that causes it not to append *any* of the
     characters in the second string.  So XmStringNConcat() would seem to
     simply not work at all when the length isn't exactly the same as the
     length of the second string.

     Plan B is to simply realloc() the XmString to make it a bit larger,
     so that we know that it's safe to go off the end.

     The NULLs at the beginning of the data we append probably aren't
     necessary, but they are read and parsed as a length, even though the
     data is not used (with the current structure of the XmStringNConcat()
     code.)  (The initialization of this memory is only necessary at all
     to keep Purify from (rightly) squawking about it.)

     And oh, by the way, the strncpy() down there, which I have verified
     is doing strncpy( <the-right-place> , <the-string-below>, 15 )
     actually writes 15 \000 bytes instead of four \000 bytes plus the
     random text below.  So I seem to have stumbled across a bug in
     strncpy() as well, even though it doesn't hose me.  THIS TIME. -- jwz
       */
#define MOTIF_BUG_BUFFER "\000\000\000\000 MOTIF BUG"
#ifdef MOTIF_BUG_BUFFER
      xmtail = (XmString) realloc ((void *) xmtail,
                   size + sizeof (MOTIF_BUG_BUFFER));
      strncpy (((char *) xmtail) + size, MOTIF_BUG_BUFFER,
           sizeof (MOTIF_BUG_BUFFER));
# undef MOTIF_BUG_BUFFER
#endif /* MOTIF_BUG_BUFFER */

      xmcombo = XmStringNConcat (xmhead, xmtail, size);
    }
  else if (xmhead)
    {
      xmcombo = xmhead;
      xmhead = 0;
    }
  else if (xmtail)
    {
      xmcombo = xmtail;
      xmtail = 0;
    }
  else
    {
      xmcombo = XmStringCreateLtoR ("", XmFONTLIST_DEFAULT_TAG);
    }

  if (xmhead) XmStringFree (xmhead);
  if (xmtail) XmStringFree (xmtail);
  return (xmcombo);
}
//////////////////////////////////////////////////////////////////////////
void 
XFE_RDFMenuToolbarBase::setItemLabelString(Widget item,HT_Resource entry)
{
    XP_ASSERT( XfeIsAlive(item) );
    XP_ASSERT( entry != NULL );

    XP_ASSERT( XmIsLabel(item) || 
               XmIsLabelGadget(item) ||
               XfeIsLabel(item) );

    MWContext * context = _frame->getContext();
    INTL_CharSetInfo charSetInfo =
        LO_GetDocumentCharacterSetInfo(context);

    // Create am XmString from the entry
    XmString xmname = 
        XFE_RDFMenuToolbarBase::entryToXmString(entry, charSetInfo);
    
    if (xmname != NULL)
    {
        XtVaSetValues(item,XmNlabelString,xmname,NULL);

        XmStringFree(xmname);
    }
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFMenuToolbarBase::notify(HT_Resource n, HT_Event whatHappened)
{
  D(debugEvent(n, whatHappened,"MTB"););
  switch (whatHappened) {
  case HT_EVENT_VIEW_SELECTED:
      _ht_view = HT_GetSelectedView(_ht_pane);
    break;
  case HT_EVENT_VIEW_REFRESH:
      updateRoot();
    break;
  default:
      // Fall through and let the base class handle this.
    break;
  }
  XFE_RDFBase::notify(n, whatHappened);
}
//////////////////////////////////////////////////////////////////////
