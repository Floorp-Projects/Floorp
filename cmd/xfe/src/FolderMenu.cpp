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
   Folder.cpp -- class for doing the dynamic folder menus
   Created: Chris Toshok <toshok@netscape.com>, 19-Dec-1996.
 */



#include "FolderMenu.h"
#include "MNView.h"
#include "MozillaApp.h"
#include "felocale.h"

#include <Xm/LabelG.h>
#include <Xm/SeparatoG.h>
#include <Xfe/Xfe.h>



#include "xpgetstr.h"
extern int XFE_FOLDER_MENU_FILE_HERE;

#define MORE_BUTTON_NAME	"mailNewsMoreButton"
#define MORE_PULLDOWN_NAME	"mailNewsMorePulldown"

typedef struct FolderCons {
	XFE_FolderMenu *menu;
	MSG_FolderInfo *info;
} FolderCons;

XFE_FolderMenu::XFE_FolderMenu(CommandType cmd, Widget w, XFE_Frame *frame)
{
  m_cascade = w;
  m_parentFrame = frame;
  m_cmd = cmd;

  XtAddCallback(m_cascade, XmNdestroyCallback, destroy_cb, this);
  
  XtVaGetValues(m_cascade,
		XmNsubMenuId, &m_submenu,
		NULL);

  XtVaGetValues(m_submenu,
		XmNnumChildren, &m_firstslot,
		NULL);

  XP_ASSERT(m_submenu);

  XFE_MozillaApp::theApp()->registerInterest(XFE_MNView::foldersHaveChanged,
					     this,
					     (XFE_FunctionNotification)foldersHaveChanged_cb);

  // make sure we initially install an update callback
  foldersHaveChanged(NULL, NULL, NULL);
}

XFE_FolderMenu::~XFE_FolderMenu()
{
  XFE_MozillaApp::theApp()->unregisterInterest(XFE_MNView::foldersHaveChanged,
					       this,
					       (XFE_FunctionNotification)foldersHaveChanged_cb);
}

void
XFE_FolderMenu::generate(Widget cascade, XtPointer clientData, XFE_Frame *frame)
{
  XFE_FolderMenu *obj;
  CommandType cmd = Command::intern((char*)clientData);

  obj = new XFE_FolderMenu(cmd, cascade, frame);
}

void
XFE_FolderMenu::activate_cb(Widget, XtPointer cd, XtPointer)
{
	FolderCons *cons = (FolderCons*)cd;
	XFE_FolderMenu *obj = cons->menu;
	MSG_FolderInfo *info = cons->info;

	XP_ASSERT(info);
	
	if (obj->m_parentFrame->handlesCommand(obj->m_cmd, (void*)info))
		obj->m_parentFrame->doCommand(obj->m_cmd, (void*)info);
}

void
XFE_FolderMenu::add_folder_menu_items(Widget menu,
				      MSG_FolderInfo *info,
				      XP_Bool filing_allowed_here)
{
  int32 count;
  Arg av [20];
  int ac;
	      

  if (filing_allowed_here)
	  {
		  Widget button;
		  FolderCons *cons = XP_NEW_ZAP(FolderCons);
		  char buf[500];
		  XmString xmname;
		  char *folderFileName, *folderName;

		  folderName = (char*)MSG_GetFolderNameFromID(info);
		  if (strrchr(folderName, '/'))
			  folderFileName = strrchr(folderName, '/') + 1;
		  else
			  folderFileName = folderName;

		  PR_snprintf(buf, sizeof(buf), XP_GetString(XFE_FOLDER_MENU_FILE_HERE), folderFileName);

		  xmname = XmStringCreate(buf, XmFONTLIST_DEFAULT_TAG);
		  
		  cons->menu = this;
		  cons->info = info;

		  ac = 0;
		  XtSetArg(av[ac], XmNuserData, NULL); ac ++;
		  XtSetArg (av[ac], XmNlabelString, xmname); ac++;

		  button = XmCreatePushButtonGadget(menu, Command::getString(m_cmd), av, ac);

		  XmStringFree(xmname);
		  
		  XtAddCallback(button, XmNactivateCallback, activate_cb, cons);
		  XtAddCallback(button, XmNdestroyCallback, free_cons_cb, cons);
		  XtManageChild(button);
		  XtVaCreateManagedWidget("sep",
								  xmSeparatorGadgetClass,
								  menu,
								  XmNshadowThickness, 2,
								  NULL);
    }

  count = MSG_GetFolderChildren(fe_getMNMaster(), info, NULL, 0);
  
  if (count == 0)
	  {
		  return;
	  }
  else
	  {
		  MSG_FolderInfo **infos = (MSG_FolderInfo**)XP_CALLOC(count, sizeof(MSG_FolderInfo*));
		  int i;
		  
		  MSG_GetFolderChildren(fe_getMNMaster(), info, infos, count);
		  
		  for (i = 0; i < count; i ++)
			  {
				  XmString xmname;
				  FolderCons *cons = XP_NEW_ZAP(FolderCons);

				  // Find the last of the "More..." menu panes
				  Widget parent = getLastMoreMenu(menu);

				  XP_ASSERT( XfeIsAlive(parent) );
				  XP_ASSERT( XmIsRowColumn(parent) );

				  cons->menu = this;
				  cons->info = infos[i];

				  if (MSG_GetFolderChildren(fe_getMNMaster(), infos[i], NULL, 0) > 0)
					  {
						  Visual *v;
						  Colormap cmap;
						  Cardinal depth;
						  Widget button;
						  Widget submenu;
						  char *folderName;
						  char *folderFileName;
						  
						  XtVaGetValues (XtParent(parent), XtNvisual, &v,
										 XtNcolormap, &cmap, XtNdepth, &depth, 0);
						  
						  ac = 0;
						  XtSetArg (av[ac], XmNvisual, v); ac++;
						  XtSetArg (av[ac], XmNdepth, depth); ac++;
						  XtSetArg (av[ac], XmNcolormap, cmap); ac++;
						  submenu = XmCreatePulldownMenu (parent, "folderCascadeMenu",
														  av, ac);
						  
						  folderName = (char*)MSG_GetFolderNameFromID(infos[i]);
						  if (strrchr(folderName, '/'))
							  folderFileName = strrchr(folderName, '/') + 1;
						  else
							  folderFileName = folderName;
						  
						  xmname = XmStringCreate(folderFileName, XmFONTLIST_DEFAULT_TAG);

						  ac = 0;
						  XtSetArg (av[ac], XmNlabelString, xmname); ac++;
						  XtSetArg (av[ac], XmNsubMenuId, submenu); ac++;
						  XtSetArg (av[ac], XmNuserData, NULL); ac++;
						  button = XmCreateCascadeButtonGadget (parent, "folderCascadeButton",
																av, ac);
						  
						  XtAddCallback(button, XmNcascadingCallback, fillin_menu,
										cons);
						  XtAddCallback(button, XmNdestroyCallback, free_cons_cb, cons);
						  XtManageChild (button);
					  }
				  else
					  {
						  Widget button;
						  char *folderName;
						  char *folderFileName;
						  
						  folderName = (char*)MSG_GetFolderNameFromID(infos[i]);
						  if (strrchr(folderName, '/'))
							  folderFileName = strrchr(folderName, '/') + 1;
						  else
							  folderFileName = folderName;
						  
						  xmname = XmStringCreate(folderFileName, XmFONTLIST_DEFAULT_TAG);
						  
						  ac = 0;
						  XtSetArg(av[ac], XmNuserData, NULL); ac ++;
						  XtSetArg (av[ac], XmNlabelString, xmname); ac++;

						  button = XmCreatePushButtonGadget(parent, Command::getString(m_cmd), av, ac);
						  
						  XtAddCallback(button, XmNactivateCallback, activate_cb, cons);
						  XtAddCallback(button, XmNdestroyCallback, free_cons_cb, cons);
						  XtManageChild(button);
					  }
				  XmStringFree(xmname);
			  }
		  
		  XP_FREE(infos);
	  }
}

void
XFE_FolderMenu::fillin_menu(Widget widget, XtPointer clientData, XtPointer)
{
	FolderCons *cons = (FolderCons*)clientData;
	XFE_FolderMenu *obj = cons->menu;
	MSG_FolderInfo *info = cons->info;
	Widget menu = 0;
	XtVaGetValues(widget, XmNsubMenuId, &menu, 0);
	
	if (info)
		obj->add_folder_menu_items(menu, info, True);
	
	XtRemoveCallback(widget, XmNcascadingCallback, fillin_menu, cons);
}

void
XFE_FolderMenu::update()
{
  Widget *kids;
  int nkids;

  XtVaGetValues (m_submenu, XmNchildren, &kids, XmNnumChildren, &nkids, 0);

  if (nkids) 
    {
      kids = &(kids[m_firstslot]);
      nkids -= m_firstslot;

      XtUnmanageChildren (kids, nkids);
      XfeDestroyMenuWidgetTree(kids, nkids,False);
    }

  {
	  MSG_FolderInfo **mailfolderinfos = NULL;
	  int num_mailfolderinfos, i;

	  num_mailfolderinfos = MSG_GetFoldersWithFlag(fe_getMNMaster(), MSG_FOLDER_FLAG_MAIL, NULL, 0);
	  if (num_mailfolderinfos > 0)
		  {
			  mailfolderinfos = (MSG_FolderInfo**)XP_ALLOC(sizeof(MSG_FolderInfo*) * num_mailfolderinfos);
			  MSG_GetFoldersWithFlag(fe_getMNMaster(), MSG_FOLDER_FLAG_MAIL, mailfolderinfos, num_mailfolderinfos);
		  }
	  
	  for (i = 0; i < num_mailfolderinfos; i ++)
		  {
			  MSG_FolderLine line;

			  if (MSG_GetFolderLineById(fe_getMNMaster(), mailfolderinfos[i], &line))
				  {
					  if (line.level == 1)
						  {
							  XmString label_str = XmStringCreate((char*)(line.prettyName 
																		  ? line.prettyName : line.name),
																  XmFONTLIST_DEFAULT_TAG);
							  XtVaCreateManagedWidget("sep",
													  xmSeparatorGadgetClass,
													  m_submenu,
													  XmNshadowThickness, 2,
													  NULL);
							  XtVaCreateManagedWidget("label",
													  xmLabelGadgetClass,
													  m_submenu,
													  XmNlabelString, label_str,
													  NULL);
							  XmStringFree(label_str);
							  XtVaCreateManagedWidget("sep",
													  xmSeparatorGadgetClass,
													  m_submenu,
													  XmNshadowThickness, 2,
													  NULL);
							  
							  add_folder_menu_items(m_submenu, mailfolderinfos[i], False);
						  }
				  }
		  }

	  if (mailfolderinfos)
		  XP_FREE(mailfolderinfos);
  }

  XtRemoveCallback(m_cascade, XmNcascadingCallback, update_cb, this);
}

void
XFE_FolderMenu::update_cb(Widget, XtPointer clientData, XtPointer)
{
  XFE_FolderMenu *obj = (XFE_FolderMenu*)clientData;

  obj->update();
}

void
XFE_FolderMenu::destroy_cb(Widget, XtPointer clientData, XtPointer)
{
  XFE_FolderMenu *obj = (XFE_FolderMenu*)clientData;

  delete obj;
}

void
XFE_FolderMenu::free_cons_cb(Widget, XtPointer clientData, XtPointer)
{
	XP_FREE(clientData);
}

XFE_CALLBACK_DEFN(XFE_FolderMenu, foldersHaveChanged)(XFE_NotificationCenter *,
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

Widget 
XFE_FolderMenu::getLastMoreMenu(Widget menu)
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
XFE_FolderMenu::createMoreButton(Widget menu)
{
	XP_ASSERT( XfeIsAlive(menu) );
	XP_ASSERT( XmIsRowColumn(menu) );

	Widget		cascade;
	Widget		submenu = NULL;
	Arg			av[5];
	Cardinal	ac = 0;

	ac = 0;
	XtSetArg(av[ac],XmNvisual,		XfeVisual(menu));	ac++;
	XtSetArg(av[ac],XmNdepth,		XfeDepth(menu));	ac++;
	XtSetArg(av[ac],XmNcolormap,	XfeColormap(menu));	ac++;
	
	submenu = XmCreatePulldownMenu(menu,MORE_PULLDOWN_NAME,av,ac);

	ac = 0;
	XtSetArg(av[ac],XmNsubMenuId,	submenu);	ac++;

	cascade = XmCreateCascadeButtonGadget(menu,MORE_BUTTON_NAME,av,ac);

	return cascade;
}
//////////////////////////////////////////////////////////////////////////
