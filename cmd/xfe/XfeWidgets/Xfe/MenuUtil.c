/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/*-----------------------------------------*/
/*																		*/
/* Name:		<Xfe/MenuUtil.c>										*/
/* Description:	Menu/RowColum misc utilities source.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/XfeP.h>
#include <Xfe/ManagerP.h>
#include <Xfe/CascadeP.h>

#include <Xm/RowColumnP.h>

#include <Xm/DisplayP.h>

#include <Xm/CascadeBGP.h>
#include <Xm/CascadeBP.h>

/*----------------------------------------------------------------------*/
/*																		*/
/* Menus																*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeCascadeGetSubMenu(Widget w)
{
    assert( w != NULL );

    if (!(XmIsCascadeButton(w) || 
          XmIsCascadeButtonGadget(w) ||
          XfeIsCascade(w)))
    {
		return NULL;
    }

	if (XmIsCascadeButton(w))
	{
		return ((XmCascadeButtonWidget) w) -> cascade_button . submenu;
	}
	else if (XfeIsCascade(w))
	{
		return ((XfeCascadeWidget) w) -> xfe_cascade . sub_menu_id;
	}

	return ((XmCascadeButtonGadget) w) -> cascade_button . submenu;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeMenuPositionXY(Widget menu,Position x_root,Position y_root)
{
	XButtonPressedEvent be;

    assert( _XfeIsAlive(menu) );
    assert( XmIsRowColumn(menu) );

	/* In the motif source code, only the x_root and y_root members are used */
	be.x_root = x_root;
	be.y_root = y_root;

	XmMenuPosition(menu,&be);
}
/*----------------------------------------------------------------------*/
/* extern */ int
XfeMenuItemPositionIndex(Widget item)
{
	XmRowColumnConstraintRec * rccp;

    assert( _XfeIsAlive(item) );
    assert( XmIsRowColumn(_XfeParent(item)) );
    assert( XtIsObject(item) );

	rccp = (XmRowColumnConstraintRec *) (item -> core . constraints);

	return (int) rccp -> row_column . position_index;
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeMenuItemAtPosition(Widget menu,int position)
{
	Widget item = NULL;
#if 0
	XmRowColumnConstraintRec * rccp;

    assert( _XfeIsAlive(item) );
    assert( XmIsRowColumn(_XfeParent(item)) );

	rccp = (XmRowColumnConstraintRec *) (item -> core . constraints);

	return (int) rccp -> row_column . position_index;
#endif

	return item;
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeMenuIsFull(Widget menu)
{
	Dimension	screen_height;
	Widget		last;

    assert( _XfeIsAlive(menu) );
    assert( XmIsRowColumn(menu) );

	screen_height = XfeScreenHeight(menu);

	last = XfeChildrenGetLast(menu);

	if (!last)
	{
		return False;
	}

	/*
	 * The menu is "full" if its height plus that of a potential new child
	 * are longer than the screen height.  Assume the new child will be
	 * the same height as the last child in the menu.
	 */
	if ((_XfeHeight(menu) + (3 * _XfeHeight(last))) > XfeScreenHeight(menu))
	{
		return True;
	}

	return False;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Option menus															*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeMenuIsOptionMenu(Widget menu)
{
	return (XmIsRowColumn(menu) && (RC_Type(menu) == XmMENU_OPTION));
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeOptionMenuSetItem(Widget menu,Cardinal i)
{
	Widget submenu;

	assert( _XfeIsAlive(menu) );
	assert( XfeMenuIsOptionMenu(menu) );

	submenu = RC_OptionSubMenu(menu);

	if (_XfeIsAlive(submenu))
	{
		if ((i >= 0) && (i < _XfemNumChildren(submenu)))
		{
			XtVaSetValues(menu,XmNmenuHistory,_XfeChildrenIndex(submenu,i),NULL);
		}
	}
}
/*----------------------------------------------------------------------*/
/* extern */ int
XfeOptionMenuGetItem(Widget menu)
{
	Cardinal	i;
	Widget		submenu;

	assert( _XfeIsAlive(menu) );
	assert( XfeMenuIsOptionMenu(menu) );

	submenu = RC_OptionSubMenu(menu);

	if (_XfeIsAlive(submenu))
	{
		for (i = 0; i < _XfemNumChildren(submenu); i++) 
		{
			if (_XfeChildrenIndex(submenu,i) == RC_MemWidget(menu))
			{
				return (int) i;
			}
		}
	}

	return -1;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*
 *
 * Traverse a menu hierarchy 
 *
 *
 *																	
 *----------------------------------------------------------------------*/
/* extern */ Widget
XfeMenuGetMoreButton(Widget menu,String more_button_name)
{
	Widget last;

    assert( _XfeIsAlive(menu) );
    assert( XmIsRowColumn(menu) );
    assert( more_button_name != NULL );

	last = XfeChildrenGetLast(menu);

	if (_XfeIsAlive(last) && 
		(XmIsCascadeButton(last) || XmIsCascadeButtonGadget(last)) &&
		(strcmp(XtName(last),more_button_name) == 0))
	{
		return last;
	}

	return NULL;
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeMenuFindLastMoreMenu(Widget menu,String more_button_name)
{
	Widget more_button;
	Widget submenu;

    assert( _XfeIsAlive(menu) );
    assert( XmIsRowColumn(menu) );
    assert( more_button_name != NULL );

	/* First check if this menu is not full */
	if (!XfeMenuIsFull(menu))
	{
		return menu;
	}

	/* Look for the "More..." button */
	more_button = XfeMenuGetMoreButton(menu,more_button_name);

	/* If no more button is found, then this menu is the last one */
	if (!more_button)
	{
		return menu;
	}

	/* Get the submenu for the more button */
	submenu = XfeCascadeGetSubMenu(more_button);

	assert( _XfeIsAlive(submenu) );

	/* Recurse into the submenu */
	return XfeMenuFindLastMoreMenu(submenu,more_button_name);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Display grabbed access.												*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeDisplayIsUserGrabbed(Widget w)
{
	XmDisplayRec *	xm_display;

	assert( _XfeIsAlive(w) );

	xm_display = (XmDisplayRec *) XmGetXmDisplay(XtDisplay(w));

	assert( xm_display != NULL );

	return xm_display->display.userGrabbed;	
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeDisplaySetUserGrabbed(Widget w,Boolean grabbed)
{
	XmDisplayRec *	xm_display;

	assert( _XfeIsAlive(w) );

	xm_display = (XmDisplayRec *) XmGetXmDisplay(XtDisplay(w));

	assert( xm_display != NULL );

	xm_display->display.userGrabbed = grabbed;
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Menu item functions.													*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeMenuItemNextItem(Widget item)
{
	int			position_index;
	Widget		parent;
	Cardinal	num_children;
	Widget		next = NULL;

    assert( _XfeIsAlive(item) );
    assert( XtIsObject(item) );

	parent = _XfeParent(item);

    assert( XmIsRowColumn(_XfeParent(item)) );

	position_index = XfeMenuItemPositionIndex(item);

	num_children = _XfemNumChildren(parent);

	if (position_index < (num_children - 1))
	{
		next = _XfeChildrenIndex(parent,position_index + 1);
	}

	return next;
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeMenuItemPreviousItem(Widget item)
{
	int			position_index;
	Widget		parent;
	Cardinal	num_children;
	Widget		previous = NULL;

    assert( _XfeIsAlive(item) );
    assert( XtIsObject(item) );

	parent = _XfeParent(item);

    assert( XmIsRowColumn(_XfeParent(item)) );

	position_index = XfeMenuItemPositionIndex(item);

	num_children = _XfemNumChildren(parent);

	if (position_index > 0)
	{
		previous = _XfeChildrenIndex(parent,position_index - 1);
	}

	return previous;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Menu type															*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ unsigned char
XfeMenuType(Widget menu)
{
    assert( _XfeIsAlive(menu) );
    assert( XmIsRowColumn(menu) );

	return RC_Type(menu);
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Destruction															*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeDestroyMenuWidgetTree(WidgetList	children,
						 int		num_children,
						 Boolean	skip_private_components)
{
    int i;

    if ((num_children <= 0) || (children == NULL))
	{
		return;
	}
	
    for (i = num_children - 1; i >= 0; i--) 
	{
		Boolean		skip = False;
		Widget		submenu = NULL;
		WidgetList	more_children = NULL;
		int			num_more_children = 0;
		
		XtVaGetValues (children[i], XmNsubMenuId, &submenu, 0);

		/* If a submenu exists, then this item has descendants */
		if (submenu) 
		{
			XtVaGetValues(children[i],
						  XmNchildren,		&more_children,
						  XmNnumChildren,	&num_more_children,
						  NULL);

			if (num_more_children > 0) 
			{
				XfeDestroyMenuWidgetTree(more_children,
										 num_more_children,
										 skip_private_components);
			}
		}

		/*
		 * Skip destruction of this widget if its requested and its
		 * parent is an XfeManager and its a private component - not
		 * meant to be messed with.
		 */
		skip = (skip_private_components &&
				XfeIsAlive(_XfeParent(children[i])) &&
				XfeIsManager(_XfeParent(children[i])) &&
				_XfeManagerPrivateComponent(children[i]));
		
		if (!skip)
		{
			XtDestroyWidget(children[i]);
		}
    }
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeMenuCreateCascadeItem(Widget			menu,
						 Widget			pulldown,
						 String			cascade_name,
						 WidgetClass	cascade_widget_class,
						 Boolean		manage_cascade,
						 ArgList		cascade_av,
						 Cardinal		cascade_ac)
{
	Widget			cascade = NULL;
	Arg				av[5];
	Cardinal		ac = 0;

	ArgList			joined_av = NULL;
	Cardinal		joined_ac = 0;

	Boolean			free_joined_av = False;

	assert( XfeIsAlive(menu) );
	assert( XmIsRowColumn(menu) );

	assert(	cascade_name != NULL );
	assert(	cascade_widget_class != NULL );

	ac = 0;

	if (_XfeIsAlive(pulldown))
	{
		XtSetArg(av[ac],XmNsubMenuId,	pulldown);	ac++;
	}

	if (cascade_ac > 0)
	{
		assert( cascade_av != NULL );

		if (ac > 0)
		{
			joined_av = XtMergeArgLists(cascade_av,cascade_ac,av,ac);
			joined_ac = cascade_ac + ac;

			free_joined_av = True;
		}
		else
		{
			joined_av = cascade_av;
			joined_ac = cascade_ac;
		}
	}
	else
	{
		joined_av = av;
		joined_ac = ac;
	}

	/* Create the cascade button */
	cascade = XtCreateWidget(cascade_name,
							 cascade_widget_class,
							 menu,
							 joined_av,
							 joined_ac);
	
	/* Manage it if needed */
	if (manage_cascade)
	{
		XtManageChild(cascade);
	}

	/* Cleanup allocated memory if needed */
	if (free_joined_av)
	{
		XtFree((char *) joined_av);
	}

	return cascade;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeMenuCreatePulldownPane(Widget		menu,
						  Widget		visual_widget,
						  String		cascade_name,
						  String		pulldown_name,
						  WidgetClass	cascade_widget_class,
						  Boolean		manage_cascade,
						  ArgList		cascade_av,
						  Cardinal		cascade_ac,
						  Widget *		cascade_out,
						  Widget *		pulldown_out)
{
	Widget	cascade = NULL;
	Widget	pulldown = NULL;

	assert( XfeIsAlive(menu) );
	assert( XfeIsAlive(visual_widget) );

	assert( XmIsRowColumn(menu) );

	assert( cascade_out != NULL );
	assert( pulldown_out != NULL );

	assert(	cascade_name != NULL );
	assert( cascade_widget_class != NULL );

	assert( XfeVisual(visual_widget) != NULL );
	assert( XfeDepth(visual_widget) > 0 );
	assert( XfeColormap(visual_widget) != None );


	/* Create the pulldown only if a name is given for it */
	if (pulldown_name != NULL)
	{
		Arg			av[5];
		Cardinal	ac = 0;

		XtSetArg(av[ac],XmNvisual,		XfeVisual(visual_widget));		ac++;
		XtSetArg(av[ac],XmNdepth,		XfeDepth(visual_widget));		ac++;
		XtSetArg(av[ac],XmNcolormap,	XfeColormap(visual_widget));	ac++;

		pulldown = XmCreatePulldownMenu(menu,pulldown_name,av,ac);
	}

	/* Create the cascade button */
	cascade = XfeMenuCreateCascadeItem(menu,
									   pulldown,
									   cascade_name,
									   cascade_widget_class,
									   manage_cascade,
									   cascade_av,
									   cascade_ac);

	*cascade_out = cascade;
	*pulldown_out = pulldown;
}
/*----------------------------------------------------------------------*/
