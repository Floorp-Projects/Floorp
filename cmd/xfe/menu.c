/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   menu.c --- menu creating utilities
   Created: Terry Weissman <terry@netscape.com>, 14-Jun-95.
 */


#include "mozilla.h"
#include "xfe.h"
#include "menu.h"

static void
free_closure_cb(Widget w, XtPointer clientData, XtPointer callData)
{
	XP_FREE(clientData);
}

int
fe_CreateSubmenu(Widget menu, struct fe_button *buttons, MWContext *context,
			XtPointer closure, XtPointer data)
{
	int			ac /*, ac1*/;
	Arg			av[20] /*, av1[5]*/;
	/*Widget			submenu;*/
	Colormap		cmap;
	Cardinal		depth;
	Visual			*v;
	int			w, step;

	if (!buttons || !buttons[0].name)
	{
		return 1;
	}

	v = 0;
	cmap = 0;
	depth = 0;
	w = 0;
	while (buttons[w].name)
	{
	    Widget menu_button = 0;
	    step = 1;
	    if (!*buttons[w].name) {	/* "" means do a line */
		ac = 0;
		menu_button = XmCreateSeparatorGadget(menu, "separator",
						      av, ac);
	    }
#ifdef __sgi
	    else if ((!strcmp("sgi", buttons[w].name) ||
		      !strcmp("adobe", buttons[w].name))
		     && !fe_VendorAnim) {
		w++;
		continue;
	    }
#endif
	    else {
		char *name = buttons[w].name;
		/*void *arg;*/
		ac = 0;

		/* don't fool the xfe2 code into thinking that the user data is a
		 * menu spec or something... */
		XtSetArg(av[ac], XmNuserData, NULL); ac++;

		if (buttons[w].type == SELECTABLE ||
		      buttons[w].type == UNSELECTABLE) {
			fe_button_closure *button_closure = XP_NEW_ZAP(fe_button_closure);
			button_closure->context = context;
			button_closure->userdata = buttons[w].userdata;

		    if (buttons[w].type == UNSELECTABLE)
				XtSetArg(av[ac], XtNsensitive, False), ac++;

		    menu_button = XmCreatePushButtonGadget(menu, name, av, ac);
			
		    XtAddCallback(menu_button,
						  XmNactivateCallback,
						  buttons[w].callback,
						  button_closure);
			XtAddCallback(menu_button,
						  XmNdestroyCallback,
						  free_closure_cb, button_closure);
		}
		else if (buttons[w].type == TOGGLE ||
			 buttons[w].type == RADIO) {
			XP_ASSERT(0);
		}
		else if (buttons[w].type == INTL_CASCADE) {
			XP_ASSERT(0);
		} else if (buttons[w].type == CASCADE) {
			XP_ASSERT(0);
		} else {
		    abort();
		}
	    }
	    if (buttons[w].offset) {
		Widget* foo = (Widget*)
				(((char*) data) + (int) (buttons[w].offset));
		*foo = menu_button;
	    }
	    XtManageChild(menu_button);
	    w+=step;
	}
	return w+1;
}


Widget
fe_PopulateMenubar(Widget parent, struct fe_button* buttons,
		   MWContext* context, void* closure, void* data,
		   XtCallbackProc pulldown_cb)
{
    Widget menubar;
    Widget menu;
    Widget menubar_button;
    Widget kids[20];
    int i;
    Arg av[20];
    int ac;
    Visual *v = 0;
    Colormap cmap = 0;
    Cardinal depth = 0;
    int w = 0;
    int step;
    char *menu_name;

    XtVaGetValues(XtParent(parent), XtNvisual, &v, XtNcolormap, &cmap,
		  XtNdepth, &depth, 0);
    
    ac = 0;
    XtSetArg(av[ac], XmNskipAdjust, True); ac++;
    XtSetArg(av[ac], XmNseparatorOn, False); ac++;
    XtSetArg(av[ac], XmNvisual, v); ac++;
    XtSetArg(av[ac], XmNdepth, depth); ac++;
    XtSetArg(av[ac], XmNcolormap, cmap); ac++;
    menubar = XmCreateMenuBar(parent, "menuBar", av, ac);

    i = 0;			/* Which menu we're on. */
    while (buttons[w].name) {
	menu_name = buttons[w].name;
	ac = 0;
	XtSetArg(av[ac], XmNvisual, v); ac++;
	XtSetArg(av[ac], XmNcolormap, cmap); ac++;
	XtSetArg(av[ac], XmNdepth, depth); ac++;
	menu = XmCreatePulldownMenu(menubar, menu_name, av, ac);
	ac = 0;
	XtSetArg(av[ac], XmNsubMenuId, menu); ac++;

#if 1
	/* Create XmCreateCascadeButton always as we want to call action
	   CleanupMenubar() later and this is available only with the
	   Button and not with the Gadget. */
	menubar_button = XmCreateCascadeButton(menubar, menu_name, av, ac);
#else /* 1 */
#ifdef _HPUX_SOURCE
	menubar_button = XmCreateCascadeButton      (menubar, menu_name, av, ac);
#else /* !HPUX */
	menubar_button = XmCreateCascadeButtonGadget(menubar, menu_name, av, ac);
#endif /* !HPUX */
#endif /* 1 */

#ifdef _HPUX_SOURCE
	/* 
	   In order to break the legs off of the HP color management
	   garbage (which we needed to do in order to not get
	   brown-on-brown widgets) we had to put the application class
	   in all of the color specifications in the resource file.
	   BUT, that's not enough for the menubar, oh no.  HP uses a
	   different pallette for that, and consequently you can't set
	   the color of it using a resource no matter what you do -
	   even if the entire path to the RowColumn is fully
	   qualified.

	   So on HPs, we make the buttons in the menubar be Widgets
	   instead of Gadgets (so that they can have a background
	   color) and then force the background color of the menubar
	   to be the background color of the first menu button on it.  */

	if (i == 0) {
	    Pixel bg = 0, tsc = 0, bsc = 0;
	    XtVaGetValues(menubar_button,
			  XmNbackground, &bg,
			  XmNtopShadowColor, &tsc,
			  XmNbottomShadowColor, &bsc,
			  0);
	    XtVaSetValues(menubar,
			  XmNbackground, bg,
			  XmNtopShadowColor, tsc,
			  XmNbottomShadowColor, bsc,
			  0);
	}
#endif

	if (buttons[w].offset) {
	    Widget* foo = (Widget*)
		(((char*) data) + (int) (buttons[w].offset));
	    *foo = menu;
	}

	XtAddCallback(menubar_button, XmNcascadingCallback,
		      pulldown_cb, (XtPointer) closure);

	/*
	 *    Allow menu by menu callbacks, instead of calling
	 *    back on every menu pulldown (slow).
	 */
	if (buttons[w].callback)
	    XtAddCallback(
			  menubar_button,
			  XmNcascadingCallback,
			  buttons[w].callback,
			  (XtPointer)closure
			  );

	kids[i] = menubar_button;

	w++;
	step = fe_CreateSubmenu(menu, &buttons[w], context, closure, data);
	if (!strcmp("help", menu_name)) {
	    XtVaSetValues(menubar, XmNmenuHelpWidget, menubar_button, 0);
	}
	w+=step;
	i++;
    }
    XtManageChildren(kids, i);
    return menubar;
}
