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
   History.cpp -- class for doing the dynamic history menus
   Created: Chris Toshok <toshok@netscape.com>, 19-Dec-1996.
 */



#include "HistoryMenu.h"
#include "HTMLView.h"
#include "felocale.h"
#include "intl_csi.h"

#include "xpgetstr.h"			// for XP_GetString()
extern int XFE_UNTITLED;

#define MAX_ITEM_WIDTH 40

typedef struct HistoryCons {
	XFE_HistoryMenu *menu;
	int entry;
} HistoryCons;

XFE_HistoryMenu::XFE_HistoryMenu(Widget w, XFE_Frame *frame)
{
  m_cascade = w;
  m_parentFrame = frame;

  XtAddCallback(m_cascade, XmNdestroyCallback, destroy_cb, this);
  
  XtVaGetValues(m_cascade,
		XmNsubMenuId, &m_submenu,
		NULL);

  XtVaGetValues(m_submenu,
		XmNnumChildren, &m_firstslot,
		NULL);

  XP_ASSERT(m_submenu);

  m_parentFrame->registerInterest(XFE_HTMLView::newURLLoading,
				  this,
				  (XFE_FunctionNotification)historyHasChanged_cb);

  // make sure we initially install an update callback
  historyHasChanged(NULL, NULL, NULL);
}

XFE_HistoryMenu::~XFE_HistoryMenu()
{
  m_parentFrame->unregisterInterest(XFE_HTMLView::newURLLoading,
				    this,
				    (XFE_FunctionNotification)historyHasChanged_cb);
}

void
XFE_HistoryMenu::generate(Widget cascade, XtPointer /*clientData*/, XFE_Frame *frame)
{
  XFE_HistoryMenu *obj;

  obj = new XFE_HistoryMenu(cascade, frame);
}

void
XFE_HistoryMenu::activate_cb(Widget, XtPointer cd, XtPointer)
{
	HistoryCons *cons = (HistoryCons*)cd;
	XFE_HistoryMenu *obj = cons->menu;
	MWContext *context = obj->m_parentFrame->getContext();
	History_entry *h;
	URL_Struct *url;
	int index = cons->entry;

	if (index < 0) return;
	
	h = SHIST_GetObjectNum (&context->hist, index);
	
	if (! h) return;
	
	url = SHIST_CreateURLStructFromHistoryEntry (context, h);
	
	if (obj->m_parentFrame->handlesCommand(xfeCmdOpenUrl, (void*)url))
		obj->m_parentFrame->doCommand(xfeCmdOpenUrl, (void*)url);
}

void
XFE_HistoryMenu::arm_cb(Widget, XtPointer cd, XtPointer)
{	
	HistoryCons *cons = (HistoryCons*)cd;
	XFE_HistoryMenu *obj = cons->menu;
	MWContext *context = obj->m_parentFrame->getContext();
	History_entry *h;
	char *address;
	int index = cons->entry;

	if (index < 0) return;
	
	h = SHIST_GetObjectNum (&context->hist, index);
	
	if (! h) return;
	
	address = h->address;
	
	if (!address) return;
	
	obj->m_parentFrame->notifyInterested(XFE_View::statusNeedsUpdatingMidTruncated, (void*)address);
}

void
XFE_HistoryMenu::disarm_cb(Widget widget, XtPointer cd, XtPointer)
{
	HistoryCons *cons = (HistoryCons*)cd;
	XFE_HistoryMenu *obj = cons->menu;
	MWContext *context = obj->m_parentFrame->getContext();
	History_entry *h;
	char *address;
	int index = cons->entry;

	XtVaGetValues (widget, XmNuserData, &index, 0);
	
	if (index < 0) return;
	
	h = SHIST_GetObjectNum (&context->hist, index);
	
	if (! h) return;
	
	address = h->address;
	
	if (!address) return;
	
	obj->m_parentFrame->notifyInterested(XFE_View::statusNeedsUpdatingMidTruncated, (void*)"");
}

void
XFE_HistoryMenu::update()
{
  MWContext *context = m_parentFrame->getContext();
  XP_List *history = SHIST_GetList (context);
  XP_List *h;
  History_entry *current;
  History_entry *e;
  int i;
//  int cpos = 0;
  int length = 0;
  int menu_length;
  Widget *kids = 0;
  Cardinal nkids = 0;
  Arg av [20];
  int ac;
  int max_length = 30;	   /* spurious statistics up 15% this year. */
  XmFontList font_list;
  INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);

  if (! history) return;
  length = XP_ListCount (history);

  history = history->next;	/* How stupid!!! */
  current = SHIST_GetCurrent (&context->hist);

  XtUnrealizeWidget (m_submenu);

  XtVaGetValues (m_submenu, XmNchildren, &kids, XmNnumChildren, &nkids, 0);

  if (nkids)
    {
      kids = &(kids[m_firstslot]);
      nkids -= m_firstslot;
     
      if (nkids)
        { 
          XtUnmanageChildren (kids, nkids);
      
          fe_DestroyWidgetTree(kids, nkids);
        }
    }

  if (length > max_length)
    menu_length = max_length;
  else
    menu_length = length;

  h = history;

  /* i is the history index and starts at 1.
     pos is the index in the menu, with 0 being the first (topmost) item. */
  for (i = length; i > 0; i--)
	  {
		  int pos = length - i;
		  XmString label;
		  char *title, *url;
		  char name [1024];
		  Widget button = NULL;
		  
		  e = SHIST_GetObjectNum (&context->hist, i);
		  if (! e) abort ();
		  
		  title = e->title;
		  url = e->address;
		  
		  if (title && !strcmp (title, XP_GetString(XFE_UNTITLED)))
			  title = 0;   /* You losers, don't do that to me! */
		  
		  ac = 0;
		  /* NULL in userData so the menu updating stuff will use XtName for the command string */
		  XtSetArg (av [ac], XmNuserData, NULL); ac++;

		  /* The pulldown menu */
		  if (pos == menu_length - 1 && menu_length < length)
			  {
				  button = XmCreateLabelGadget (m_submenu, "historyTruncated", av, ac);
			  }
		  else if (pos < menu_length)
			  {
				  HistoryCons *cons = XP_NEW_ZAP(HistoryCons);

				  cons->menu = this;
				  cons->entry = i;

				  fe_FormatDocTitle (title, url, name, 1024);
				  INTL_MidTruncateString (INTL_GetCSIWinCSID(c), name, name,
										  MAX_ITEM_WIDTH);
				  
				  label = fe_ConvertToXmString ((unsigned char *) name,
												INTL_GetCSIWinCSID(c), NULL, XmFONT_IS_FONT, &font_list);
				  XtSetArg (av [ac], XmNlabelString, label); ac++;
				  if (e == current)
					  {
						  XtSetArg (av [ac], XmNset, True); ac++;
						  XtSetArg (av [ac], XmNvisibleWhenOff, False); ac++;
						  XtSetArg (av [ac], XmNindicatorType, XmN_OF_MANY); ac++;
						  button = XmCreateToggleButtonGadget(m_submenu, xfeCmdOpenUrl, av, ac);
						  // must be xfeCmdOpenUrl or sensitivity becomes a problem
						  XtAddCallback (button, XmNvalueChangedCallback,
										 activate_cb, cons);
						  XtAddCallback (button, XmNarmCallback,
										 arm_cb, cons);
						  XtAddCallback (button, XmNdisarmCallback,
										 disarm_cb, cons);
					  }
				  else
					  {
						  button = XmCreatePushButtonGadget (m_submenu, xfeCmdOpenUrl, av, ac);
						  XtAddCallback (button, XmNarmCallback, arm_cb,
										 cons);
						  XtAddCallback (button, XmNdisarmCallback, disarm_cb,
										 cons);
						  XtAddCallback (button, XmNactivateCallback, activate_cb,
										 cons);
					  }

				  XtAddCallback(button, XmNdestroyCallback, free_cons_cb, cons);

				  XmStringFree (label);
			  }

		  if (button != NULL)
		  {
			  XtManageChild(button);
		  }
		  
// 		  if (e == current)
// 			  cpos = length - i + 1;
		  h = h->next;
	  }
  
  XtRealizeWidget (m_submenu);
  XtRemoveCallback(m_cascade, XmNcascadingCallback, update_cb, this);
}

void
XFE_HistoryMenu::update_cb(Widget, XtPointer clientData, XtPointer)
{
  XFE_HistoryMenu *obj = (XFE_HistoryMenu*)clientData;

  obj->update();
}

void
XFE_HistoryMenu::destroy_cb(Widget, XtPointer clientData, XtPointer)
{
  XFE_HistoryMenu *obj = (XFE_HistoryMenu*)clientData;

  delete obj;
}

void
XFE_HistoryMenu::free_cons_cb(Widget, XtPointer clientData, XtPointer)
{
	XP_FREE(clientData);
}

XFE_CALLBACK_DEFN(XFE_HistoryMenu, historyHasChanged)(XFE_NotificationCenter *,
						      void *,
						      void *)
{
  // This may seem stupid, but it keeps us from having more than
  // one reference to this particular callback without having
  // to worry about other cascadingCallbacks.

  // remove it if it's already there
  XtRemoveCallback(m_cascade, XmNcascadingCallback, update_cb, this);

  // and then add it back.
  XtAddCallback(m_cascade, XmNcascadingCallback, update_cb, this);
}
