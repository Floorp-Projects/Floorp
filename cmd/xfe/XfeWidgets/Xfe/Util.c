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
/* Name:		<Xfe/Util.h>											*/
/* Description:	Xfe widgets misc utilities source.						*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <stdio.h>
#include <stdlib.h>

#include <Xfe/XfeP.h>

#include <Xfe/PrimitiveP.h>
#include <Xfe/ManagerP.h>

#include <Xm/LabelP.h>
#include <Xm/LabelGP.h>
#include <Xfe/LabelP.h>

#if XmVersion >= 2000
#include <Xm/Gadget.h>
#include <Xm/Primitive.h>
#include <Xm/Manager.h>
#endif

#define MESSAGE0 "XfeInstancePointer() called with non XfePrimitive or XfeManager widget."

/*----------------------------------------------------------------------*/
/*																		*/
/* Simple Widget access functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeWindowedWidget(Widget w)
{
    assert( _XfeIsAlive(w) );

	if (!_XfeIsAlive(w))
	{
		return NULL;
	}

    return (XmIsGadget(w) ? XtParent(w) : w);
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeIsViewable(Widget w)
{
    XWindowAttributes xwa;

	if (!_XfeIsAlive(w) || !XtIsWidget(w) || !_XfeIsRealized(w))
	{
		return False;
	}

    if (XGetWindowAttributes(XtDisplay(w),_XfeWindow(w),&xwa))
    {
		return (xwa.map_state == IsViewable);
    }
    
    return False;
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeIsAlive(Widget w)
{
    return _XfeIsAlive(w);
}
/*----------------------------------------------------------------------*/
/* extern */ XtPointer
XfeUserData(Widget w)
{
    XtPointer user_data = NULL;
    
    assert( w != NULL );
    assert(XmIsGadget(w) || XmIsPrimitive(w) || XmIsManager(w));
    
    if (XmIsPrimitive(w))
    {
		user_data = _XfeUserData(w);
    }
    else if (XmIsManager(w))
    {
		user_data = _XfemUserData(w);
    }
    else if (XmIsGadget(w))
    {
		XtVaGetValues(w,XmNuserData,&user_data,NULL);
    }
    
    return user_data;
}
/*----------------------------------------------------------------------*/
/* extern */ XtPointer
XfeInstancePointer(Widget w)
{
    assert( w != NULL );
    assert( XfeIsPrimitive(w) || XfeIsManager(w) );
    
    if (XfeIsPrimitive(w))
    {
		return _XfeInstancePointer(w);
    }
    else
    {
		return _XfemInstancePointer(w);
    }

	_XfeWarning(w,MESSAGE0);

	return NULL;
}
/*----------------------------------------------------------------------*/
/* extern */ Colormap
XfeColormap(Widget w)
{
    assert( _XfeIsAlive(w) );
    
    return _XfeColormap(w);
}
/*----------------------------------------------------------------------*/
/* extern */ Cardinal
XfeDepth(Widget w)
{
    assert( _XfeIsAlive(w) );
    
    return _XfeDepth(w);
}
/*----------------------------------------------------------------------*/
/* extern */ Pixel
XfeBackground(Widget w)
{
    assert( _XfeIsAlive(w) );

	if (!_XfeIsAlive(w))
	{
		return 0;
	}
    
    return _XfeBackgroundPixel(w);
}
/*----------------------------------------------------------------------*/
/* extern */ Pixel
XfeForeground(Widget w)
{
    assert( _XfeIsAlive(w) );

	if (!_XfeIsAlive(w))
	{
		return 0;
	}
    
	if (XmIsPrimitive(w))
	{
		return _XfeForeground(w);
	}

	return _XfemForeground(w);
}
/*----------------------------------------------------------------------*/
/* extern */ String
XfeClassNameForWidget(Widget w)
{
	assert( w != NULL );

	return _XfeClassName(_XfeClass(w));
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Simple WidgetClass access functions									*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ String
XfeClassName(WidgetClass wc)
{
	assert( wc != NULL );

	return _XfeClassName(wc);
}
/*----------------------------------------------------------------------*/
/* extern */ WidgetClass
XfeSuperClass(WidgetClass wc)
{
	assert( wc != NULL );

	return _XfeSuperClass(wc);
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* XmFontList															*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ XmFontList
XfeXmFontListCopy(Widget w,XmFontList font_list,unsigned char font_type)
{
    XmFontList new_font_list;
    
    /* If the font is null, create a default one */
    if (!font_list)
    {
		new_font_list = XmFontListCopy(_XmGetDefaultFontList(w,font_type));
    }
    /* Otherwise make a carbon copy */
    else
    {
		new_font_list = XmFontListCopy(font_list);
    }
    
    return new_font_list;
}
/*----------------------------------------------------------------------*/
/*																		*/
/* This function is useful when effecient access to a widget's 			*/
/* XmNfontList is needed.  It avoids the GetValues() and				*/
/* XmStringCopy() overhead.  Of course, the result should be 			*/
/* considered read only.												*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ XmFontList
XfeFastAccessFontList(Widget w)
{
	XmFontList	font_list = NULL;

	assert( _XfeIsAlive(w) );

	if (!_XfeIsAlive(w))
	{
		return NULL;
	}

	if (XmIsLabel(w))
	{
		font_list = ((XmLabelWidget) w) -> label . font;
	}
	else if (XmIsLabelGadget(w))
	{
		font_list = ((XmLabelGadget) w) -> label . font;
	}
	else if (XfeIsLabel(w))
	{
		font_list = ((XfeLabelWidget) w) -> xfe_label . font_list;
	}
#ifdef DEBUG_ramiro
	else
	{
		assert( 0 );
	}
#endif

	return font_list;
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Colors																*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Pixel
XfeSelectPixel(Widget w,Pixel base)
{
    Pixel	select_pixel;
    Widget	core_widget = XfeWindowedWidget(w);
    
    XmGetColors(_XfeScreen(core_widget),_XfeColormap(core_widget),
				base,NULL,NULL,NULL,&select_pixel);
    
    return select_pixel;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Explicit invocation of core methods.  								*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeExpose(Widget w,XEvent * event,Region region)
{
	WidgetClass wc;

	assert( _XfeIsAlive(w) );
	assert( XtClass(w) != NULL );

	if (!_XfeIsAlive(w))
	{
		return;
	}

	wc = XtClass(w);
	
	assert( wc->core_class.expose != NULL );

	(*wc->core_class.expose)(w,event,region);
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeResize(Widget w)
{
	WidgetClass wc;

	assert( _XfeIsAlive(w) );
	assert( XtClass(w) != NULL );

	if (!_XfeIsAlive(w))
	{
		return;
	}

	wc = XtClass(w);
	
	assert( wc->core_class.resize != NULL );

	(*wc->core_class.resize)(w);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Sleep routine.														*/
/*																		*/
/*----------------------------------------------------------------------*/
static void
SleepTimeout(XtPointer client_data,XtIntervalId * id)
{
 	Boolean * sleeping = (Boolean *) client_data;

 	*sleeping = False;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeSleep(Widget w,XfeEventLoopProc proc,int ms)
{
 	Boolean		sleeping = True;

	assert( _XfeIsAlive(w) );
	assert( proc != NULL );

	XtAppAddTimeOut(XtWidgetToApplicationContext(w),
					ms,
					SleepTimeout,
					(XtPointer) &sleeping);	

	/* Invoke the user's event loop while we are sleeping */
	while(sleeping)
	{
		(*proc)();
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/* extern */ void 
XfeRectSet(XRectangle *	rect,
		   Position		x,
		   Position		y,
		   Dimension	width,
		   Dimension	height)
{
	assert( rect != NULL );

	rect->x			= x;
	rect->y			= y;
	rect->width		= width;
	rect->height	= height;
}
/*----------------------------------------------------------------------*/
/* extern */ void 
XfeRectCopy(XRectangle * dst,XRectangle * src)
{
	assert( dst != NULL );
	assert( src != NULL );

	dst->x			= src->x;
	dst->y			= src->y;
	dst->width		= src->width;
	dst->height		= src->height;
}
/*----------------------------------------------------------------------*/
/* extern */ void 
XfePointSet(XPoint *	point,
			Position	x,
			Position	y)
{
	assert( point != NULL );

	point->x	= x;
	point->y	= y;
}
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfePointInRect(XRectangle *	rect,int x,int y)
{
	assert( rect != NULL );

	return ( (x >= rect->x) &&
			 (x <= (rect->x + rect->width)) &&
			 (y >= rect->y) &&
			 (y <= (rect->y + rect->height)) );
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Management															*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeSetManagedState(Widget w,Boolean state)
{
	if (!_XfeIsAlive(w))
	{
		return;
	}

	if (state)
	{
		XtManageChild(w);
	}
	else
	{
		XtUnmanageChild(w);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeToggleManagedState(Widget w)
{
	if (!_XfeIsAlive(w))
	{
		return;
	}

	if (_XfeIsManaged(w))
	{
		XtUnmanageChild(w);
	}
	else
	{
		XtManageChild(w);
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XEvent functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeEventGetXY(XEvent * event,int * x_out,int * y_out)
{
	Boolean	result = False;
	int		x = 0;
	int		y = 0;

	assert( x_out || y_out );
	
	if (event)
	{
		if (event->type == EnterNotify || event->type == LeaveNotify)
		{
			x = event->xcrossing.x;
			y = event->xcrossing.y;

			result = True;
		}
		else if (event->type == MotionNotify)
		{
			x = event->xmotion.x;
			y = event->xmotion.y;

			result = True;
		}
		else if (event->type == ButtonPress || event->type == ButtonRelease)
		{
			x = event->xbutton.x;
			y = event->xbutton.y;

			result = True;
		}
#ifdef DEBUG_ramiro
		else
        {
          printf("Unknown event type '%d'\n",event->type);

          assert( 0 );
        }
#endif
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
/* extern */ Boolean
XfeEventGetRootXY(XEvent * event,int * x_out,int * y_out)
{
	Boolean	result = False;
	int		x = 0;
	int		y = 0;

	assert( x_out || y_out );
	
	if (event)
	{
		if (event->type == EnterNotify || event->type == LeaveNotify)
		{
			x = event->xcrossing.x_root;
			y = event->xcrossing.y_root;

			result = True;
		}
		else if (event->type == MotionNotify)
		{
			x = event->xmotion.x_root;
			y = event->xmotion.y_root;

			result = True;
		}
		else if (event->type == ButtonPress || event->type == ButtonRelease)
		{
			x = event->xbutton.x_root;
			y = event->xbutton.y_root;

			result = True;
		}
#ifdef DEBUG_ramiro
		else
        {
          printf("Unknown event type '%d'\n",event->type);

          assert( 0 );
        }
#endif
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
/* extern */ Modifiers
XfeEventGetModifiers(XEvent * event)
{
	if (event)
	{
		/*
		 * There probably a better way to do this using unions.
		 * But, I want to limit which events are checked.
		 */
		if (event->type == ButtonPress || event->type == ButtonRelease)
		{
			return event->xbutton.state;
		}
		else if (event->type == MotionNotify)
		{
			return event->xmotion.state;
		}
		else if (event->type == KeyPress || event->type == KeyRelease)
		{
			return event->xkey.state;
		}
		else if (event->type == EnterNotify || event->type == LeaveNotify)
		{
			return event->xcrossing.state;
		}
#ifdef DEBUG_ramiro
        else
        {
          printf("Unknown event type '%d'\n",event->type);

          assert( 0 );
        }
#endif
	}

	return 0;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Test whether a widget is a private component of an XfeManager parent */
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeIsPrivateComponent(Widget w)
{
	return (XfeIsManager(_XfeParent(w)) && 
			_XfeManagerPrivateComponent(w));
}
/*----------------------------------------------------------------------*/

