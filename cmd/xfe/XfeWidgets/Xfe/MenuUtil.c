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
#include <Xfe/BmCascadeP.h>
#include <Xfe/BmButtonP.h>

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

    if (!XmIsCascadeButton(w) || 
        !XmIsCascadeButtonGadget(w) || 
        !XfeIsCascade(w))
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
/* Private accent functions.											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define ACCENT_DRAW		1
#define ACCENT_ERASE	2

static void		AccentBottom	(Widget,GC,GC,Dimension,Dimension,
								 Dimension,Dimension,int);
static void		AccentAll		(Widget,GC,GC,Dimension,Dimension,
								 Dimension,Dimension,int);
static void		AccentTop		(Widget,GC,GC,Dimension,Dimension,
								 Dimension,Dimension,int);

/*----------------------------------------------------------------------*/
/*																		*/
/* Public accent drawing functions.										*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeMenuItemDrawAccent(Widget			item,
					  unsigned char		accent_type,
					  Dimension			offset_left,
					  Dimension			offset_right,
					  Dimension			shadow_thickness,
					  Dimension			accent_thickness)
{
	Widget		pw;
	GC			top_gc;
	GC			bottom_gc;
	XGCValues	values;
	int			top_subwindow_mode;
	int			bottom_subwindow_mode;

	/* duh */
	if (accent_type == XmACCENT_NONE)
	{
		return;
	}

	assert( _XfeIsAlive(item) );
	assert( XmIsPushButton(item) || XmIsCascadeButton(item) );

	pw = _XfeParent(item);

	assert( XmIsRowColumn(pw) );

	top_gc		= _XfemTopShadowGC(pw);
	bottom_gc	= _XfemBottomShadowGC(pw);

	/* Remember the old wubwindow mode values */
	XGetGCValues(XtDisplay(item),top_gc,GCSubwindowMode,&values);

	top_subwindow_mode = values.subwindow_mode;

	XGetGCValues(XtDisplay(item),bottom_gc,GCSubwindowMode,&values);

	bottom_subwindow_mode = values.subwindow_mode;

	/* Force the subwindow mode to IncludeInferiors */
	XSetSubwindowMode(XtDisplay(item),top_gc,IncludeInferiors);
	XSetSubwindowMode(XtDisplay(item),bottom_gc,IncludeInferiors);

	switch(accent_type)
	{
	case XmACCENT_BOTTOM:
		AccentBottom(item,top_gc,bottom_gc,offset_left,offset_right,
					 shadow_thickness,accent_thickness,ACCENT_DRAW);
		break;

	case XmACCENT_ALL:
		AccentAll(item,top_gc,bottom_gc,offset_left,offset_right,
				  shadow_thickness,accent_thickness,ACCENT_DRAW);
		break;

	case XmACCENT_TOP:
		AccentTop(item,top_gc,bottom_gc,offset_left,offset_right,
				  shadow_thickness,accent_thickness,ACCENT_DRAW);
		break;

	default:
		break;
	}

	/* Restore the old subwindow mode */
	XSetSubwindowMode(XtDisplay(item),top_gc,top_subwindow_mode);
	XSetSubwindowMode(XtDisplay(item),bottom_gc,bottom_subwindow_mode);
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeMenuItemEraseAccent(Widget			item,
					   unsigned char	accent_type,
					   Dimension		offset_left,
					   Dimension		offset_right,
					   Dimension		shadow_thickness,
					   Dimension		accent_thickness)
{
	Widget		pw;
	GC			gc;
	XGCValues	values;
	int			subwindow_mode;

	/* duh */
	if (accent_type == XmACCENT_NONE)
	{
		return;
	}

	assert( _XfeIsAlive(item) );
	assert( XmIsPushButton(item) || XmIsCascadeButton(item) );

	pw = _XfeParent(item);

	assert( XmIsRowColumn(pw) );

	gc = _XfemBackgroundGC(pw);

	/* Remember the old wubwindow mode values */
	XGetGCValues(XtDisplay(item),gc,GCSubwindowMode,&values);

	subwindow_mode = values.subwindow_mode;

	/* Force the subwindow mode to IncludeInferiors */
	XSetSubwindowMode(XtDisplay(item),gc,IncludeInferiors);

	switch(accent_type)
	{
	case XmACCENT_BOTTOM:
		AccentBottom(item,gc,None,offset_left,offset_right,
					 shadow_thickness,accent_thickness,ACCENT_ERASE);
		break;

	case XmACCENT_ALL:
		AccentAll(item,gc,None,offset_left,offset_right,
				  shadow_thickness,accent_thickness,ACCENT_ERASE);
		break;

	case XmACCENT_TOP:
		AccentTop(item,gc,None,offset_left,offset_right,
				  shadow_thickness,accent_thickness,ACCENT_ERASE);
		break;

	default:
		break;
	}

	/* Restore the old subwindow mode */
	XSetSubwindowMode(XtDisplay(item),gc,subwindow_mode);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Private accent functions.											*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
AccentBottom(Widget			w,
			 GC				top_gc,
			 GC				bottom_gc,
			 Dimension		offset_left,
			 Dimension		offset_right,
			 Dimension		shadow_thickness,
			 Dimension		accent_thickness,
			 int			code)
{
	Widget		pw = _XfeParent(w);
	int			position_index = XfeMenuItemPositionIndex(w);
	Cardinal	num_children = _XfemNumChildren(pw);
	Position	x;
	Position	y;
	Dimension	width;
	Dimension	height = 2 * shadow_thickness + accent_thickness;

	assert( code == ACCENT_DRAW || code == ACCENT_ERASE );

	/* Last item */
	if (position_index == (num_children - 1))
	{
		x = _XfeX(w) + offset_left;
		y = _XfeY(w) + _XfeHeight(w) - height;
		
		width = _XfeWidth(w) - offset_left - offset_right;
	}
	/* Any other item */
	else
	{
		x = _XfeX(w) + offset_left;
		y = _XfeY(w) + _XfeHeight(w) - height / 2;
		
		width = _XfeWidth(w) - offset_left - offset_right;
	}

	if (code == ACCENT_DRAW)
	{
		/* Draw accent */
		_XmDrawShadows(XtDisplay(pw),_XfeWindow(pw),top_gc,bottom_gc,
					   x,y,width,height,shadow_thickness,XmSHADOW_IN);
	}
	else
	{
		/* Erase accent */
		XfeDrawRectangle(XtDisplay(pw),_XfeWindow(pw),top_gc,
						 x,y,width,height,shadow_thickness);
	}
}
/*----------------------------------------------------------------------*/
static void
AccentTop(Widget			w,
		  GC				top_gc,
		  GC				bottom_gc,
		  Dimension			offset_left,
		  Dimension			offset_right,
		  Dimension			shadow_thickness,
		  Dimension			accent_thickness,
		  int				code)
{
	Widget		pw = _XfeParent(w);
	int			position_index = XfeMenuItemPositionIndex(w);
	Position	x;
	Position	y;
	Dimension	width;
	Dimension	height = 2 * shadow_thickness + accent_thickness;

	assert( code == ACCENT_DRAW || code == ACCENT_ERASE );

	/* First item */
	if (position_index == 0)
	{
		x = _XfeX(w) + offset_left;
		y = _XfeY(w);
		
		width = _XfeWidth(w) - offset_left - offset_right;
	}
	/* Any other item */
	else
	{
		x = _XfeX(w) + offset_left;
		y = _XfeY(w) - height / 2;
		
		width = _XfeWidth(w) - offset_left - offset_right;
	}

	if (code == ACCENT_DRAW)
	{
		/* Draw accent */
		_XmDrawShadows(XtDisplay(pw),_XfeWindow(pw),top_gc,bottom_gc,
					   x,y,width,height,shadow_thickness,XmSHADOW_IN);
	}
	else
	{
		/* Erase accent */
		XfeDrawRectangle(XtDisplay(pw),_XfeWindow(pw),top_gc,
						 x,y,width,height,shadow_thickness);
	}
}
/*----------------------------------------------------------------------*/
static void
AccentAll(Widget			w,
		  GC				top_gc,
		  GC				bottom_gc,
		  Dimension			offset_left,
		  Dimension			offset_right,
		  Dimension			shadow_thickness,
		  Dimension			accent_thickness,
		  int				code)
{
#if 1	
	if (code == ACCENT_DRAW)
	{
		/* Draw accent */
		_XmDrawShadows(XtDisplay(w),_XfeWindow(w),
					   top_gc,bottom_gc,
					   _XfeHighlightThickness(w),
					   _XfeHighlightThickness(w),
					   _XfeWidth(w) - 2 * _XfeHighlightThickness(w),
					   _XfeHeight(w) - 2 * _XfeHighlightThickness(w),
					   _XfeShadowThickness(w),
					   XmSHADOW_OUT);
	}
	else
	{
		/* Erase accent */
		XfeDrawRectangle(XtDisplay(w),_XfeWindow(w),top_gc,
						 _XfeHighlightThickness(w),
						 _XfeHighlightThickness(w),
						 _XfeWidth(w) - 2 * _XfeHighlightThickness(w),
						 _XfeHeight(w) - 2 * _XfeHighlightThickness(w),
						 _XfeShadowThickness(w));
	}
#else
	Widget		pw = _XfeParent(w);
	int			position_index = XfeMenuItemPositionIndex(w);
	Cardinal	num_children = _XfemNumChildren(pw);
	Position	x;
	Position	y;
	Dimension	width;
	Dimension	height;
	Dimension	total_thickness = shadow_thickness + accent_thickness;

	assert( code == ACCENT_DRAW || code == ACCENT_ERASE );

	/* One and only */
	if (num_children == 1)
	{
		x = _XfeX(w) + offset_left;
		y = _XfeY(w);

		width = _XfeWidth(w) - offset_left - offset_right;
		height = _XfeHeight(w);
	}
	else
	{
		Dimension overlap = (2 * shadow_thickness + accent_thickness) / 2;

		/* First item */
		if (position_index == 0)
		{
			x = _XfeX(w) + offset_left;
			y = _XfeY(w);
			
			width = _XfeWidth(w) - offset_left - offset_right;

			height = _XfeHeight(w) + overlap;
		}
		/* Last item */
		else if (position_index == (num_children - 1))
		{
			x = _XfeX(w) + offset_left;
			y = _XfeY(w) - overlap;
			
			width = _XfeWidth(w) - offset_left - offset_right;

			height = _XfeHeight(w) + overlap;
		}
		/* In between two others */
		else
		{
			x = _XfeX(w) + offset_left;
			y = _XfeY(w) - overlap;
			
			width = _XfeWidth(w) - offset_left - offset_right;

			height = _XfeHeight(w) + 2 * total_thickness;
		}
	}

	if (code == ACCENT_DRAW)
	{
		/* Draw accent */
		_XmDrawShadows(XtDisplay(pw),_XfeWindow(pw),top_gc,bottom_gc,
				   x,y,width,height,shadow_thickness,XmSHADOW_IN);
		
		x += total_thickness;
		y += total_thickness;
		
		width -= (2 * total_thickness);
		height -= (2 * total_thickness);
				   
		_XmDrawShadows(XtDisplay(pw),_XfeWindow(pw),top_gc,bottom_gc,
					   x,y,width,height,shadow_thickness,XmSHADOW_OUT);
	}
	else
	{
		/* Erase accent */
		XfeDrawRectangle(XtDisplay(pw),_XfeWindow(pw),top_gc,
						 x,y,width,height,shadow_thickness);
		
		x += total_thickness;
		y += total_thickness;
		
		width -= (2 * total_thickness);
		height -= (2 * total_thickness);
				   
		XfeDrawRectangle(XtDisplay(pw),_XfeWindow(pw),top_gc,
						 x,y,width,height,shadow_thickness);
	}
#endif
}
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
XfeDestroyMenuWidgetTree(WidgetList		children,
						 int			num_children,
						 Boolean		skip_private_components)
{
	int			i;

	if ((num_children <= 0) || (children == NULL))
	{
		return;
	}
    
	for (i = num_children - 1; i >= 0; i--) 
	{
		Boolean		skip = False;
/* 		Widget		submenu = XfeCascadeGetSubMenu(children[i]); */
 		Widget		submenu;

		XtVaGetValues(children[i],XmNsubMenuId,&submenu,NULL);

		if (submenu) 
		{
			/*
			 * I think we should use submenu instead of children[i],
             * but this is how the original code was written.
			 *
			 * I need to think about this some more...
             */
			if (_XfemChildren(children[i]) && _XfemNumChildren(children[i]))
			{
				XfeDestroyMenuWidgetTree(_XfemChildren(children[i]),
										 _XfemNumChildren(children[i]),
										 skip_private_components);
			}
		}
		
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
