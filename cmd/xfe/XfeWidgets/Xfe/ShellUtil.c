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

/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		<Xfe/ShellUtil.h>										*/
/* Description:	Shell misc utilities source.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#include <Xfe/XfeP.h>
#include <Xfe/Cascade.h>

#include <Xm/AtomMgr.h>
#include <Xm/Protocols.h>

#include <X11/ShellP.h>

/*----------------------------------------------------------------------*/
/*																		*/
/* Only the Shell widgets have visuals									*/
/*																		*/
/*----------------------------------------------------------------------*/
Visual *
XfeVisual(Widget w)
{
	Widget shell;

	assert( w != NULL );

	shell = XfeAncestorFindByClass(w,shellWidgetClass,XfeFIND_ANY);

	assert( _XfeIsAlive(shell) );

	if (!_XfeIsAlive(w))
	{
		return NULL;
	}

	return ((ShellWidget) shell) -> shell . visual;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc shell functions													*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeShellIsPoppedUp(Widget shell)
{
    assert( XtIsShell(shell) );

	if (!_XfeIsAlive(shell) || !_XfeIsRealized(shell))
	{
		return False;
	}

	return ((ShellWidget) shell)->shell.popped_up;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeShellAddCloseCallback(Widget			shell,
						 XtCallbackProc	callback,
						 XtPointer		data)
{
	static Atom wm_delete_window = None;

    assert( XtIsShell(shell) );

	if (!_XfeIsAlive(shell))
	{
		return;
	}

	if (wm_delete_window == None)
	{
		wm_delete_window = XmInternAtom(XtDisplay(shell),
										"WM_DELETE_WINDOW",
										False);
	}
		
	XmAddWMProtocolCallback(shell,wm_delete_window,callback,data);
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeShellRemoveCloseCallback(Widget			shell,
							XtCallbackProc	callback,
							XtPointer		data)
{
	static Atom wm_delete_window = None;

    assert( XtIsShell(shell) );

	if (!_XfeIsAlive(shell))
	{
		return;
	}

	if (wm_delete_window == None)
	{
		wm_delete_window = XmInternAtom(XtDisplay(shell),
										"WM_DELETE_WINDOW",
										False);
	}
		
	XmRemoveWMProtocolCallback(shell,wm_delete_window,callback,data);
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeShellGetDecorationOffset(Widget			shell,
							Position *		x_out,
							Position *		y_out)
{
	Position	x = 0;
	Position	y = 0;
	Boolean		result = False;

    assert( XtIsShell(shell) );
    assert( x_out != NULL || y_out != NULL );

	if (_XfeIsAlive(shell))
	{
		Window			root;
		Window			parent = None;
		Window			parent2 = None;
		Window *		children = NULL;
		unsigned int 	num_children;
		
		if (!XQueryTree(XtDisplay(shell),_XfeWindow(shell),
						&root,&parent,&children,&num_children))
		{
			parent2 = None;
		}
		
		if (children)
		{
			XFree(children);

			children = NULL;
		}

		if (parent != None)
		{
			int				px;
			int				py;
			unsigned int	width;
			unsigned int	height;
			unsigned int	border;
			unsigned int	depth;
			
			if (XGetGeometry(XtDisplay(shell),parent,&root,&px,&py,
							 &width,&height,&border,&depth))
			{
				x = px;
				y = py;

				result = True;
			}
		}
	}

	if (x_out)
	{
		*x_out = x;
	}

	if (y_out)
	{
		*y_out = y;
	}

	return result;
}
/*----------------------------------------------------------------------*/
/* extern */ int
XfeShellGetGeometryFromResource(Widget			shell,
								Position *		x_out,
								Position *		y_out,
								Dimension *		width_out,
								Dimension *		height_out)
{
	int				mask = 0;
	int				x = 0;
	int				y = 0;
	unsigned int	width = 0;
	unsigned int	height = 0;

    assert( XtIsShell(shell) );

	if (_XfeIsAlive(shell))
	{
		String geometry = (String) XfeGetValue(shell,XmNgeometry);

		if (geometry)
		{
			mask = XParseGeometry(geometry,&x,&y,&width,&height);

			if (mask & XValue)
			{
				if (mask & XNegative)
				{
					x = - XfeAbs(x);
				}
			}
			else
			{
				x = 0;
			}

			if (mask & YValue)
			{
				if (mask & YNegative)
				{
					y = - XfeAbs(y);
				}
			}
			else
			{
				y = 0;
			}

			if (!(mask & WidthValue))
			{
				width = 0;
			}

			if (!(mask & HeightValue))
			{
				height = 0;
			}
		}
	}

	if (x_out)
	{
		*x_out = (Position) x;
	}

	if (y_out)
	{
		*y_out = (Position) y;
	}

	if (width_out)
	{
		*width_out = (Dimension) width;
	}

	if (height_out)
	{
		*height_out = (Dimension) height;
	}

	return mask;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeShellSetIconicState(Widget shell,Boolean state)
{
  /*
	Because of weirdness in Xt/Shell.c, one can't change the "iconic"
	flag of a shell after it has been created but before it has been realized.
	That resource is only checked when the shell is Initialized (ie, when
	XtCreatePopupShell is called) instead of the more obvious times like:
	when it is Realized (before the window is created) or when it is Managed
	(before window is mapped).
	
	The Shell class's SetValues method does not modify wm_hints.initial_state
	if the widget has not yet been realized - it ignores the request until
	afterward, when it's too late.
   */

    assert( XtIsWMShell(shell) );

	if (_XfeIsAlive(shell))
	{
		WMShellWidget wms = (WMShellWidget) shell;
		
        XfeSetValue(shell,XtNiconic,state);
		
		wms->wm.wm_hints.flags |= StateHint;
		wms->wm.wm_hints.initial_state = (state ? IconicState : NormalState);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeShellGetIconicState(Widget shell)
{
	Boolean state = False;

    assert( XtIsWMShell(shell) );

	if (_XfeIsAlive(shell))
	{
		state = (Boolean) XfeGetValue(shell,XtNiconic);
	}

	return state;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeShellPlaceAtLocation(Widget			shell,
						Widget			relative,
						unsigned char	location,
						Dimension		dx,
						Dimension		dy)
{
	int rx;
	int ry;

	switch(location)
	{
	case XmLOCATION_EAST:
		break;

	case XmLOCATION_NORTH:
		break;

	case XmLOCATION_NORTH_EAST:
		break;

	case XmLOCATION_NORTH_WEST:
		break;

	case XmLOCATION_SOUTH:
		break;

	case XmLOCATION_SOUTH_EAST:
		break;

	case XmLOCATION_SOUTH_WEST:
		break;

	case XmLOCATION_WEST:
		break;

	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeRectPlaceOnRootWindow(Widget			relative,
						 Dimension		offset,
						 Dimension		margin_bottom,
						 Dimension		margin_top,
						 Dimension		margin_left,
						 Dimension		margin_right,
						 XRectangle *	rect_in_out,
                         unsigned char	location,
						 Boolean		allow_swap)
{
	XRectangle r;
	
	Dimension	max_width;
	Dimension	max_height;
	Dimension	screen_width;
	Dimension	screen_height;
	Position	max_x;
	Position	max_y;
	Position	min_x;
	Position	min_y;

	r.x			= rect_in_out->x;
	r.y			= rect_in_out->y;
	r.width		= rect_in_out->width;
	r.height	= rect_in_out->height;

	screen_width  = XfeScreenWidth(relative);
	screen_height = XfeScreenHeight(relative);

	max_width  = screen_width - margin_left - margin_right;
	max_height = screen_height - margin_top - margin_bottom;

	min_x = margin_left;
	min_y = margin_top;

	max_x = screen_width - margin_right;
	max_y = screen_height - margin_bottom;

	/* Make sure the width fits */
	if (rect_in_out->width > max_width)
	{
		rect_in_out->width = max_width;
	}

	/* Make sure the height fits */
	if (rect_in_out->height > max_height)
	{
		rect_in_out->height = max_height;
	}

	/* Place horizontally */
	if (location == XmSHELL_PLACEMENT_BOTTOM ||
		location == XmSHELL_PLACEMENT_TOP)
	{
		rect_in_out->x = XfeRootX(relative);

		/* Make sure the rect does not go over the right border */
		if ((rect_in_out->x + rect_in_out->width) > max_x)
		{
			rect_in_out->x -= ((rect_in_out->x + rect_in_out->width) - max_x);
		}

		if (location == XmSHELL_PLACEMENT_BOTTOM)
		{
			rect_in_out->y = 
				XfeRootY(relative) +
				XfeHeight(relative) +
				offset;				

			/* Make sure the rect does not go over the bottom border */
			if ((rect_in_out->y + rect_in_out->height) > max_y)
			{
				rect_in_out->height -= ((rect_in_out->y + rect_in_out->height) - 
										max_y);
			}
		}
	}
	/* Place vertically */
	else if (location == XmSHELL_PLACEMENT_LEFT ||
			 location == XmSHELL_PLACEMENT_RIGHT)
	{
		rect_in_out->y = XfeRootY(relative);
		
		/* Make sure the rect does not go over the bottom border */
		if ((rect_in_out->y + rect_in_out->height) > max_y)
		{
			rect_in_out->y -= ((rect_in_out->y + rect_in_out->height) - max_y);
		}
	}
#ifdef DEBUG_ramiro
	else
	{
		assert( 0 );
	}
#endif

	switch(location)
	{
	case XmSHELL_PLACEMENT_BOTTOM:

		
		
		break;

	case XmSHELL_PLACEMENT_LEFT:
		break;

	case XmSHELL_PLACEMENT_RIGHT:
		break;

	case XmSHELL_PLACEMENT_TOP:
		break;
	}
}
/*----------------------------------------------------------------------*/
