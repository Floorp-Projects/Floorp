/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "TaskBarManager.h"

#include "TaskBar.h"

#include <Xm/MwmUtil.h>      // For MWM_DECOR_BORDER
#include <X11/IntrinsicP.h>  // For XtMoveWidget

// Was fe_TaskBarManager_Create()
XFE_TaskBarManager::XFE_TaskBarManager(MWContext *context, Pixel bg)
{
    // We need to add notification to set this
    m_biffState = MSG_BIFF_Unknown;
    // A preference will eventually determine this state
    // as well as the position
    m_isFloat = False;
    m_taskBarList = XP_ListNew();

    Widget shell = createFloatPalette(context);

    m_floatTaskBar = new XFE_TaskBar(shell, NULL, TRUE);

    if ( m_isFloat )
        XtPopup (shell, XtGrabNone);
}

// Was fe_TaskBarManager_Register()
// Used to create icons in here for some reason
void
XFE_TaskBarManager::Register(XFE_TaskBar *taskb)
{
   if (!taskb ) return;

   XP_ListAddObject(m_taskBarList, taskb);
}

void
XFE_TaskBarManager::Unregister(XFE_TaskBar *taskb)
{
    if (!taskb) return;

    XP_ListRemoveObject(m_taskBarList, taskb);
}

void
XFE_TaskBarManager::DoDock()
{
    // Instead of doing these searching gymnastics,
    //   should we just make a member to hold onto the shell?  -slamm
    Widget shell = m_floatForm;
    while (shell && !XtIsShell(shell)) {
        shell = XtParent(m_floatForm);
    }
    if ( shell ) XtPopdown(shell);

    int count = XP_ListCount(m_taskBarList);

    XFE_TaskBar *taskp;
    for ( int i = 1; i <= count; i++ )
    {
        taskp = (XFE_TaskBar *)
            XP_ListGetObjectNum(m_taskBarList, i);

        taskp->Dock();
    }
    m_isFloat = FALSE;
}

void
XFE_TaskBarManager::DoFloat(Widget w)
{
  Position x,y;
  Position rootX, rootY;
  Dimension width, height;

  XtVaGetValues(w, XmNx, &x, XmNy, &y, 
                XmNheight, &height, XmNwidth, &width, 
                0);

  XtTranslateCoords(w, x, y, &rootX, &rootY);

  int count = XP_ListCount(m_taskBarList);

  XFE_TaskBar *taskp = 0;
  for ( int i = 1; i <= count; i++ )
  {
      taskp = (XFE_TaskBar*)XP_ListGetObjectNum(m_taskBarList, i);
      taskp->Undock();
  }

   Widget shell = m_floatForm;
   while (shell && !XtIsShell(shell)) shell = XtParent(shell);
   XtMoveWidget(shell, rootX, rootY + height);
   if ( shell ) 
      XtPopup (shell, XtGrabNone);

   m_isFloat = TRUE;
}

void
XFE_TaskBarManager::RaiseFloat()
{
   Widget shell; 

   shell = m_floatForm;
   while (shell && !XtIsShell(shell)) shell = XtParent(shell);
   if ( shell ) 
      XtPopup (shell, XtGrabNone);
}

void
XFE_TaskBarManager::UpdateBiffState(MSG_BIFF_STATE biffState) 
{
    m_biffState = biffState;

    // Update them icons;
    
    int count = XP_ListCount(m_taskBarList);

    XFE_TaskBar *taskp;
    for ( int i = 1; i <= count; i++ )
    {
        taskp = (XFE_TaskBar *)
            XP_ListGetObjectNum(m_taskBarList, i);

        taskp->UpdateBiffState(biffState);
    }
    m_floatTaskBar->UpdateBiffState(biffState);
}

// Was fe_TaskBarManager_CreateFloatPalette
Widget
XFE_TaskBarManager::createFloatPalette(MWContext *context)
{
   Widget mainw = CONTEXT_WIDGET(context);
   Widget toplevel = XtParent(mainw);
   Widget shell;
   Arg av[20];
   int ac = 0;
   Visual *v = 0;
   Colormap cmap = 0;
   Cardinal depth = 0;
   Widget controlW;
   Widget rowcol;
   Widget form;

   XtVaGetValues(mainw, XtNvisual, &v, 
		XtNcolormap, &cmap, XtNdepth, &depth, 0 );

   ac = 0;
   XtSetArg(av[ac], XmNvisual, v); ac++;
   XtSetArg(av[ac], XmNdepth, depth); ac++;
   XtSetArg(av[ac], XmNcolormap, cmap); ac++;
   XtSetArg(av[ac], XmNautoUnmanage, False); ac++;
   XtSetArg(av[ac], XmNdeleteResponse, XmDESTROY); ac++;
   XtSetArg(av[ac], XmNallowShellResize, True); ac++;
   XtSetArg(av[ac], XmNwidth, 160); ac++;
   XtSetArg(av[ac], XmNheight, 36); ac++;
   XtSetArg(av[ac], XmNmwmDecorations, 
		MWM_DECOR_BORDER|MWM_DECOR_TITLE ); ac++;
   XtSetArg(av[ac], XmNwindowGroup, XtWindow(mainw)); ac++;
   shell = XtCreatePopupShell("FloatIconBox", topLevelShellWidgetClass,
			toplevel, av, ac);

   XtAddCallback(shell, XmNdestroyCallback, 
                 XFE_TaskBarManager::destroy_cb, this);

   XtRealizeWidget(shell);
   
   return shell;
}

// Was fe_TaskBarManager_DestroyCB
// static callback
void
XFE_TaskBarManager::destroy_cb(Widget w, XtPointer clientData, 
                               XtPointer CallData)
{
  MWContext *context = 0;
  struct fe_MWContext_cons *cons;

  XFE_TaskBarManager *taskBMgr = (XFE_TaskBarManager *)CallData;

  /* Use the first available browser context as the context */
  cons = fe_all_MWContexts;
  if ( cons )
  {
    context = cons->context;
    taskBMgr->createFloatPalette(context);
    taskBMgr->DoDock();
  }
}

