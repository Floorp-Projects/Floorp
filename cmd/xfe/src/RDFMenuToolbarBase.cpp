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
#include "Frame.h"
#include "IconGroup.h"
#include "View.h"
#include "ToolbarDrop.h"

#include "RDFUtils.h"
#include "MenuUtils.h"

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
}
//////////////////////////////////////////////////////////////////////////
/* virtual */
XFE_RDFMenuToolbarBase::~XFE_RDFMenuToolbarBase()
{
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

    int numItems = 0;

    D(printf("Create tree: %s\n",HT_GetNodeName(entry)));
    HT_SetOpenState(entry, PR_TRUE);
    int numAlloc = HT_GetCountVisibleChildren(entry);
    Widget* wItems = (Widget*) calloc(numAlloc, sizeof(Widget));
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
            wItems[numItems++] = item;
        }
    }
    XtManageChildren(wItems, numItems);
    free(wItems);
    wItems = NULL;
    HT_DeleteCursor(child_cursor);
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFMenuToolbarBase::prepareToUpdateRoot()
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
        if (XFE_RDFUtils::ht_IsFECommand(entry)) 
        {
            CommandType cmd = XFE_RDFUtils::ht_GetFECommand(entry);

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
        else
        {
            XFE_RDFUtils::launchEntry(_frame->getContext(), entry);
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

    non_full_menu = XFE_MenuUtils::getLastMoreMenu(menu,
												   MORE_BUTTON_NAME,
												   MORE_PULLDOWN_NAME,
												   _fancyItems);

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
         || XFE_RDFUtils::ht_FolderHasFolderChildren(entry)))
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
    XFE_RDFUtils::setItemLabelString(cascade,entry);
    
    // Configure the new cascade button
	XFE_RDFUtils::configureMenuCascadeButton(cascade,entry);

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
XFE_RDFMenuToolbarBase::createPushButton(Widget menu, HT_Resource entry)
{
    XP_ASSERT( XfeIsAlive(menu) );
    XP_ASSERT( XmIsRowColumn(menu) );

    D(printf("Create push : %s\n",HT_GetNodeName(entry)));

    Widget                        non_full_menu;
    Widget                        button;
    ItemCallbackStruct *    data = NULL;

    non_full_menu = XFE_MenuUtils::getLastMoreMenu(menu,
												   MORE_BUTTON_NAME,
												   MORE_PULLDOWN_NAME,
												   _fancyItems);

    XP_ASSERT( XfeIsAlive(non_full_menu) );

    // must be xfeCmdOpenTargetUrl or sensitivity becomes a problem
    if (_fancyItems)
    {
        button = XfeCreateBmButton(non_full_menu,xfeCmdOpenTargetUrl,NULL,0);
    }
    else
    {
        button = XmCreatePushButton(non_full_menu,xfeCmdOpenTargetUrl,NULL,0);
    }

    // Configure the new push button
	XFE_RDFUtils::configureMenuPushButton(button,entry);

    // Set the item's label
    XFE_RDFUtils::setItemLabelString(button,entry);

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
XFE_RDFMenuToolbarBase::createSeparator(Widget menu)
{
    XP_ASSERT( XfeIsAlive(menu) );

    Widget     parent;        
    Widget     separator;
    String     name;

    if (XmIsRowColumn(menu))
    {
        parent = XFE_MenuUtils::getLastMoreMenu(menu,
												MORE_BUTTON_NAME,
												MORE_PULLDOWN_NAME,
												_fancyItems);

        name = MENU_PANE_SEPARATOR_NAME;
    }
    else
    {
        parent = menu;

        name = TOOL_BAR_SEPARATOR_NAME;
    }

    XP_ASSERT( XfeIsAlive(parent) );

    separator = XmCreateSeparator(parent,name,NULL,0);

//    configureSeparator(separator,NULL);

    XtManageChild(separator);
    
    return separator;
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
