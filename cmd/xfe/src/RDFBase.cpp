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
   RDFBase.cpp - Class wrapper for HT/RDF backend
                 HT Pane creation and notification management
   Created: Stephen Lamm <slamm@netscape.com>, 28-Jul-1998
 */

#include "RDFBase.h"
#include "xp_str.h"
#include "xpassert.h"

#if DEBUG_mcafee
#define D(x) x
#else
#define D(x)
#endif

//////////////////////////////////////////////////////////////////////////
//
// RDFBase 'public' methods
//
//////////////////////////////////////////////////////////////////////////

XFE_RDFBase::XFE_RDFBase()
    : _ht_pane(NULL),
      _ht_view(NULL),
      _ht_ns(NULL)
{
}
/*virtual*/
XFE_RDFBase::~XFE_RDFBase() 
{
    XFE_RDFBase *  xfe_view = (XFE_RDFBase *)HT_GetViewFEData(_ht_view);
    if (xfe_view == this) 
    {
        // HT is holding onto a pointer to this class.
        // Since this is going away, clear the pointer from HT.
        HT_SetViewFEData(_ht_view, NULL);
    }

    if (isPaneCreator())
    {
        deletePane();
    }
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFBase::newPane()
{
    startPaneCreate();

    _ht_pane = HT_NewPane(_ht_ns);

    finishPaneCreate();
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFBase::newPaneFromURL(MWContext *context,
                            char *url,
							int  param_count,
							char **param_names,
							char **param_values)
{
    startPaneCreate();

    _ht_pane = HT_PaneFromURL(context, url, NULL/*template_url*/ ,  _ht_ns, 0,
                              param_count, param_names, param_values);

    finishPaneCreate();
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFBase::newPaneFromResource(HT_Resource node)
{
    newPaneFromResource(HT_GetRDFResource(node));
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFBase::newPaneFromResource(RDF_Resource node)
{
    startPaneCreate();

    _ht_pane = HT_PaneFromResource(node, _ht_ns,
                                   PR_FALSE, PR_TRUE, PR_TRUE);

    finishPaneCreate();
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFBase::newBookmarksPane()
{
    newPane();
    HT_View view = HT_GetViewType(_ht_pane, HT_VIEW_BOOKMARK);
    HT_SetSelectedView(_ht_pane, view);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFBase::newHistoryPane()
{
    newPane();
    HT_View view = HT_GetViewType(_ht_pane, HT_VIEW_HISTORY);
    HT_SetSelectedView(_ht_pane, view);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFBase::newToolbarPane()
{
    startPaneCreate();

    _ht_pane = HT_NewToolbarPane(_ht_ns);

    finishPaneCreate();
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFBase::startPaneCreate()
{
    deletePane();

    // Setup the notification struct
    _ht_ns = new HT_NotificationStruct;
    XP_BZERO(_ht_ns, sizeof(HT_NotificationStruct));
    _ht_ns->notifyProc = notify_cb;
    _ht_ns->data = this;
}
//////////////////////////////////////////////////////////////////////
void
XFE_RDFBase::finishPaneCreate()
{
    HT_SetPaneFEData(_ht_pane, this);

    _ht_view = HT_GetSelectedView(_ht_pane);
}
//////////////////////////////////////////////////////////////////////
void
XFE_RDFBase::deletePane()
{
    if (_ht_ns)
    {
        XP_ASSERT(_ht_pane);

        if (_ht_pane)
        {
            _ht_ns->data = NULL;
            HT_DeletePane(_ht_pane);
        }

        delete _ht_ns;
        _ht_ns = NULL;
    }
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFBase::setHTView(HT_View view)
{
    XP_ASSERT(view);

    // Nothing to do
    if (view == _ht_view) return;

    _ht_view = view;

    HT_Pane pane = HT_GetPane(_ht_view);

    // It is an error to switch to a new pane in setHTView()
    // if this object is the HT_Pane creator.
    // Use the new pane methods instead.
    XP_ASSERT(!isPaneCreator() || _ht_pane == pane);

    _ht_pane = pane;

    HT_SetViewFEData(view, this);

    updateRoot();
}
//////////////////////////////////////////////////////////////////////////
XP_Bool
XFE_RDFBase::isPaneCreator()
{
    // Pane was created by this object if the notification struct is set.
    // No nofication struct means the pane was created by another object.
    return _ht_ns != NULL;
}
//////////////////////////////////////////////////////////////////////////
HT_Resource
XFE_RDFBase::getRootFolder()
{
    XP_ASSERT(_ht_pane);

    if (_ht_pane) {
        if (_ht_view == NULL) {
            // No view set.  Get the selected view from HT-backend.
            _ht_view = HT_GetSelectedView(_ht_pane);
        }
        // View are often loaded from the Net, so make sure we have
        // one before we try to use it.
        if (_ht_view) {
            return HT_TopNode(_ht_view);
        }
    }
    return NULL;
}
//////////////////////////////////////////////////////////////////////////
/*virtual*/ void
XFE_RDFBase::updateRoot()
{
}
//////////////////////////////////////////////////////////////////////////
/*static*/ XP_Bool
XFE_RDFBase::ht_IsFECommand(HT_Resource item)
{
    const char* url = HT_GetNodeURL(item);

    return (XP_STRNCMP(url, "command:", 8) == 0);
}
//////////////////////////////////////////////////////////////////////////
/*static*/ CommandType
XFE_RDFBase::ht_GetFECommand(HT_Resource item)
{
    const char* url = HT_GetNodeURL(item);

    if (url && XP_STRNCMP(url, "command:", 8) == 0)
    {
        return Command::convertOldRemote(url + 8);
    }
    else 
    {
        return NULL;
    }
}
//////////////////////////////////////////////////////////////////////////
/*virtual*/ void
XFE_RDFBase::notify(HT_Resource n, HT_Event whatHappened)
{
  D(debugEvent(n, whatHappened,"Base"););

  switch (whatHappened) {
  case HT_EVENT_VIEW_ADDED:
      ; // Do nothing
    break;
  default:
      // Pass this event to the view
      if (isPaneCreator()) 
      {
          HT_View        ht_view =   HT_GetView(n);
          XFE_RDFBase *  xfe_view =  (XFE_RDFBase *)HT_GetViewFEData(ht_view);

          if (xfe_view && xfe_view != this)
              xfe_view->notify(n, whatHappened);
      }
    break;
  }

}
//////////////////////////////////////////////////////////////////////////
//
// RDFBase 'protected' methods
//
//////////////////////////////////////////////////////////////////////////
void 
XFE_RDFBase::notify_cb(HT_Notification ns, HT_Resource n, 
                       HT_Event whatHappened, 
                       void * /*token*/, uint32 /*tokenType*/)
{
  XFE_RDFBase * xfe_rdfpane_obj = (XFE_RDFBase *)ns->data;

  // Check if we are deleting the pane
  if (xfe_rdfpane_obj == NULL)
  {  
      // Ignore the events if we are. */
      return;
  }

  // HT is so great.  It start call us with events before we have
  // even returned from the pane creation function.  Thus, _ht_pane,
  // may still be NULL.  This will fix that.
  if (xfe_rdfpane_obj->_ht_pane == NULL)
  {
      HT_View view = HT_GetView(n);
      xfe_rdfpane_obj->_ht_pane = HT_GetPane(view);
  }
  xfe_rdfpane_obj->notify(n, whatHappened);
}
//////////////////////////////////////////////////////////////////////////
#ifdef DEBUG
void
XFE_RDFBase::debugEvent(HT_Resource n, HT_Event whatHappened,
                        const char * label)
{
    HT_View view = HT_GetView(n);
    XP_ASSERT(view);
    HT_Pane pane = HT_GetPane(view);

    char *viewName = HT_GetViewName(view);
    char *nodeName = HT_GetNodeName(n);

    if (strcmp(viewName, nodeName) == 0)
        nodeName = "<same>";

    static HT_Resource      last_node    = 0;
    static HT_Event         last_event   = 0;
    static XFE_RDFBase *    last_obj     = 0;
    static int              last_count   = 0;

    if (last_node == n && 
        last_event == whatHappened &&
        last_obj == this)
    {
        last_count++;
    }
    else
    {
        last_node  = n;
        last_event = whatHappened;
        last_obj   = this;
        last_count = 0;
    }

    if (pane != _ht_pane)
    {
        for (int ii=0; ii < last_count; ii++)
            printf("    ");

        printf("%s: Warning: pane(0x%x) and _ht_pane(0x%x) do not match\n",
               label,pane,_ht_pane);
    }

    for (int ii=0; ii < last_count; ii++)
        printf("    ");

#ifdef DEBUG_slamm
#define EVENTDEBUG(x) printf("%s: %s (0x%x) %s, %s\n",\
                             label,(x),pane,viewName,nodeName);
#else
#define EVENTDEBUG(x)
#endif


  switch (whatHappened) {
  case HT_EVENT_NODE_ADDED:
    EVENTDEBUG("NODE_ADDED");
    break;
  case HT_EVENT_NODE_DELETED_DATA:
    EVENTDEBUG("NODE_DELETED_DATA");
    break;
  case HT_EVENT_NODE_DELETED_NODATA:
    EVENTDEBUG("NODE_DELETED_NODATA");
    break;
  case HT_EVENT_NODE_VPROP_CHANGED:
    EVENTDEBUG("NODE_VPROP_CHANGED");
    break;
  case HT_EVENT_NODE_SELECTION_CHANGED:
    EVENTDEBUG("NODE_SELECT");
    break;
  case HT_EVENT_NODE_OPENCLOSE_CHANGED:
    EVENTDEBUG("NODE_OPENCLOSE_CHANGED");
    break;
  case HT_EVENT_VIEW_CLOSED:
    EVENTDEBUG("VIEW_CLOSED");
    break;
  case HT_EVENT_VIEW_SELECTED:
    EVENTDEBUG("VIEW_SELECTED");
    break;
  case HT_EVENT_VIEW_ADDED:
    EVENTDEBUG("VIEW_ADDED");
    break;
  case HT_EVENT_NODE_OPENCLOSE_CHANGING:
    EVENTDEBUG("NODE_OPENCLOSE_CHANGING");
    break;
  case HT_EVENT_VIEW_SORTING_CHANGED:
    EVENTDEBUG("VIEW_SORTING_CHANGED");
    break;
  case HT_EVENT_VIEW_REFRESH:
    EVENTDEBUG("VIEW_REFRESH");
    break;
  case HT_EVENT_VIEW_WORKSPACE_REFRESH:
    EVENTDEBUG("VIEW_WORKSPACE_REFRESH");
    break;
  case HT_EVENT_NODE_EDIT:
    EVENTDEBUG("NODE_EDIT");
    break;
  case HT_EVENT_WORKSPACE_EDIT:
    EVENTDEBUG("WORKSPACE_EDIT");
    break;
  case HT_EVENT_VIEW_HTML_ADD:
    EVENTDEBUG("VIEW_HTML_ADD");
    break;
  case HT_EVENT_VIEW_HTML_REMOVE:
    EVENTDEBUG("VIEW_HTML_REMOVE");
    break;
  case HT_EVENT_NODE_ENABLE:
    EVENTDEBUG("NODE_ENABLE");
    break;
  case HT_EVENT_NODE_DISABLE:
    EVENTDEBUG("NODE_DISABLE");
    break;
  case HT_EVENT_NODE_SCROLLTO:
    EVENTDEBUG("NODE_SCROLLTO");
    break;
  case HT_EVENT_COLUMN_ADD:
    EVENTDEBUG("COLUMN_ADD");
    break;
  case HT_EVENT_COLUMN_DELETE:
    EVENTDEBUG("COLUMN_DELETE");
    break;
  case HT_EVENT_COLUMN_SIZETO:
    EVENTDEBUG("COLUMN_SIZETO");
    break;
  case HT_EVENT_COLUMN_REORDER:
    EVENTDEBUG("COLUMN_REORDER");
    break;
  case HT_EVENT_COLUMN_SHOW:
    EVENTDEBUG("COLUMN_SHOW");
    break;
  case HT_EVENT_COLUMN_HIDE:
    EVENTDEBUG("COLUMN_HIDE");
    break;
  default:
    D(printf("RDFBase: Unknown event, %d, on %s\n",
             whatHappened, HT_GetNodeName(n)));
    break;
  }
}
#endif
