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
/* Name:		<Xfe/Geometry.c>										*/
/* Description:	Xfe widgets geometry utilities.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <stdarg.h>

#include <Xfe/PrimitiveP.h>
#include <Xfe/ManagerP.h>

#if 1
#ifdef DEBUG_ramiro
#define DEBUG_DIMENSIONS 1
#endif
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* Access functions														*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Dimension
XfeBorderWidth(Widget w)
{
    assert( w != NULL );

#if DEBUG_DIMENSIONS
	{
		Dimension border_width;

		XtVaGetValues(w,XmNborderWidth,&border_width,NULL);

		assert( border_width == _XfeBorderWidth(w) );
	}
#endif

    return _XfeBorderWidth(w);
}
/*----------------------------------------------------------------------*/
/* extern */ Dimension
XfeWidth(Widget w)
{
    assert( w != NULL );

#if DEBUG_DIMENSIONS
	{
		Dimension width;

		XtVaGetValues(w,XmNwidth,&width,NULL);

		assert( width == _XfeWidth(w) );
	}
#endif
    
    return _XfeWidth(w);
}
/*----------------------------------------------------------------------*/
/* extern */ Dimension
XfeHeight(Widget w)
{
    assert( w != NULL );

#if DEBUG_DIMENSIONS
	{
		Dimension height;

		XtVaGetValues(w,XmNheight,&height,NULL);

		assert( height == _XfeHeight(w) );
	}
#endif
    
    return _XfeHeight(w);
}
/*----------------------------------------------------------------------*/
/* extern */ Dimension
XfeScreenWidth(Widget w)
{
    assert( _XfeIsAlive(w) );

	if (!_XfeIsAlive(w))
	{
		return 0;
	}

	return DisplayWidth(XtDisplay(w),XScreenNumberOfScreen(_XfeScreen(w)));
}
/*----------------------------------------------------------------------*/
/* extern */ Dimension
XfeScreenHeight(Widget w)
{
    assert( _XfeIsAlive(w) );

	if (!_XfeIsAlive(w))
	{
		return 0;
	}

	return DisplayHeight(XtDisplay(w),XScreenNumberOfScreen(_XfeScreen(w)));
}
/*----------------------------------------------------------------------*/
/* extern */ Position
XfeX(Widget w)
{
    assert( _XfeIsAlive(w) );

	if (!_XfeIsAlive(w))
	{
		return 0;
	}
    
    return _XfeX(w);
}
/*----------------------------------------------------------------------*/
/* extern */ Position
XfeY(Widget w)
{
    assert( _XfeIsAlive(w) );

	if (!_XfeIsAlive(w))
	{
		return 0;
	}
    
    return _XfeY(w);
}
/*----------------------------------------------------------------------*/
/* extern */ Position
XfeRootX(Widget w)
{
	Position root_x;

    assert( _XfeIsAlive(w) );

	if (!_XfeIsAlive(w))
	{
		return 0;
	}

	XtTranslateCoords(w,0,0,&root_x,NULL);

    return root_x;
}
/*----------------------------------------------------------------------*/
/* extern */ Position
XfeRootY(Widget w)
{
	Position root_y;

    assert( _XfeIsAlive(w) );

	if (!_XfeIsAlive(w))
	{
		return 0;
	}
	
	XtTranslateCoords(w,0,0,NULL,&root_y);

    return root_y;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Biggest, widest and tallest functions								*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeBiggestWidget(Boolean horizontal,WidgetList widgets,Cardinal n)
{
	Dimension		biggest_dim = 0;
	Dimension		dim;
	Widget			max_widget = NULL;
	Widget			widget;
	Cardinal		i;

	assert( widgets != NULL );
	
	if (!widgets)
	{
		return NULL;
	}

	for (i = 0; i < n; i++) 
	{
		widget = widgets[i];

		assert( _XfeIsAlive(widget) );

		dim = (horizontal ? _XfeWidth(widget) : _XfeHeight(widget));

		if (dim > biggest_dim) 
		{
			biggest_dim = dim;
			max_widget = widget;
		}
	}
	
	return max_widget;
}
/*----------------------------------------------------------------------*/
/* extern */ Dimension
XfeVaGetWidestWidget(Widget widget, ...)
{
	va_list		vargs;
	Widget		current;
	Dimension	width;
	Dimension	widest_width;

	/*
	 * Get the first widget's width
	 * (because of the way var args works
	 *  we have to start the loop at the second widget)
	 */
	widest_width = _XfeWidth(widget);

	/*
	 * Get the widest width
	 */
	va_start (vargs, widget);

	while ((current = va_arg (vargs, Widget))) 
	{
		width = _XfeWidth(current);

		if (width > widest_width)
		{
			widest_width = width;
		}
	}

	va_end (vargs);

	return widest_width;
}
/*----------------------------------------------------------------------*/
/* extern */ Dimension
XfeVaGetTallestWidget(Widget widget, ...)
{
	va_list		vargs;
	Widget		current;
	Dimension	height;
	Dimension	widest_height;

	/*
	 * Get the first widget's height
	 * (because of the way var args works
	 *  we have to start the loop at the second widget)
	 */
	widest_height = _XfeHeight(widget);

	/*
	 * Get the widest height
	 */
	va_start (vargs, widget);

	while ((current = va_arg (vargs, Widget))) 
	{
		height = _XfeHeight(current);

		if (height > widest_height)
		{
			widest_height = height;
		}
	}

	va_end (vargs);

	return widest_height;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Misc																	*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Dimension
_XfeHeightCenter(Widget one,Widget two)
{
	assert( one != NULL );
	assert( _XfeIsAlive(one) );
	assert( two != NULL );
	assert( _XfeIsAlive(two) );

	return
		(_XfeHeight(one) > _XfeHeight(two)) ? 
		((_XfeHeight(one) - _XfeHeight(two)) / 2) :
		0;
}
/*----------------------------------------------------------------------*/
/* extern */ Dimension
_XfeWidthCenter(Widget one,Widget two)
{
	assert( one != NULL );
	assert( _XfeIsAlive(one) );
	assert( two != NULL );
	assert( _XfeIsAlive(two) );

	return
		(_XfeWidth(one) > _XfeWidth(two)) ? 
		((_XfeWidth(one) - _XfeWidth(two)) / 2) :
		0;
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Geometry																*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeConfigureWidget(Widget w,int x,int y,int width,int height)
{
    assert( _XfeIsAlive(w) );

#if 0
	printf("_XfeConfigureWidget(%s,%d,%d,%d,%d)\n",XtName(w),x,y,width,height);
#endif

#if 0
	assert( x >= 0 );
	assert( y >= 0 );

	assert( width > 0 );
	assert( height > 0 );
#endif

	/* Ignore this request if width or height are 0 */
	if (!width || !height)
	{
		return;
	}

	/* Make sure the positions and dimensions are different */
	if ((_XfeX(w) == x) && (_XfeY(w) == y) && 
		(_XfeWidth(w) == width) && (_XfeHeight(w) == height))
	{
		return;
	}

	/* Configure XfePrimitive class */
	if (XfeIsPrimitive(w))
	{
		Boolean use_preferred_height;
		Boolean use_preferred_width;
		
		use_preferred_width = _XfeUsePreferredWidth(w);
		use_preferred_height = _XfeUsePreferredHeight(w);
		
		_XfeUsePreferredWidth(w) = False;
		_XfeUsePreferredHeight(w) = False;
		
		XtConfigureWidget(w,x,y,width,height,_XfeBorderWidth(w));
		
		_XfeUsePreferredWidth(w) = use_preferred_width;
		_XfeUsePreferredHeight(w) = use_preferred_height;
	}
	/* Configure XfeManager class */
	else if (XfeIsManager(w))
	{
		Boolean use_preferred_height;
		Boolean use_preferred_width;

		_XfemOldWidth(w) = _XfeWidth(w);
		_XfemOldHeight(w) = _XfeHeight(w);
		
		use_preferred_width = _XfemUsePreferredWidth(w);
		use_preferred_height = _XfemUsePreferredHeight(w);
		
		_XfemUsePreferredWidth(w) = False;
		_XfemUsePreferredHeight(w) = False;
		
		XtConfigureWidget(w,x,y,width,height,_XfeBorderWidth(w));
		
		_XfemUsePreferredWidth(w) = use_preferred_width;
		_XfemUsePreferredHeight(w) = use_preferred_height;
	}
	/* Configure any other class */
	else
	{
		_XmConfigureObject(w,x,y,width,height,_XfeBorderWidth(w));
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeConfigureOrHideWidget(Widget w,int x,int y,int width,int height)
{
	assert( _XfeIsAlive(w) );

	/* Make sure the widget is alive */
	if (!_XfeIsAlive(w))
	{
		return;
	}

	/* Make sure the positions and dimensions are different */
	if ((_XfeX(w) == x) && (_XfeY(w) == y) && 
		(_XfeWidth(w) == width) && (_XfeHeight(w) == height))
	{
		return;
	}

	/* Hide the widget if its dimensions are less thanb zero */
	if (!width || !height)
	{
		_XfeSetMappedWhenManaged(w,False);

		return;
	}

	_XfeConfigureWidget(w,x,y,width,height);

	/* Show the widget */
	_XfeSetMappedWhenManaged(w,True);
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeResizeWidget(Widget w,int width,int height)
{
    assert( _XfeIsAlive(w) );

#if 0
	printf("_XfeResizeWidget(%s,%d,%d)\n",XtName(w),width,height);
#endif

#if 0
	assert( width > 0 );
	assert( height > 0 );
#endif

	/* Ignore this request if width or height are 0 */
	if (!width || !height)
	{
		return;
	}

	/* Make sure the dimension are different */
	if ((_XfeWidth(w) == width) && (_XfeHeight(w) == height))
	{
		return;
	}

	_XfeConfigureWidget(w,_XfeX(w),_XfeY(w),width,height);
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfeMoveWidget(Widget w,int x,int y)
{
    assert( _XfeIsAlive(w) );

#if 0
	printf("_XfeMoveWidget(%s,%d,%d)\n",XtName(w),width,height);
#endif

#if 0
	assert( x >= 0 );
	assert( y >= 0 );
#endif

	/* Make sure the positions are different */
	if ((_XfeX(w) == x) && (_XfeY(w) == y))
	{
		return;
	}

	/* Configure XfePrimitive or XfeManager classes */
	if (XfeIsPrimitive(w) || XfeIsManager(w))
	{
		XtMoveWidget(w,x,y);
	}

	/* Configure any other class */
	else
	{
		_XmMoveObject(w,x,y);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
_XfePreferredGeometry(Widget w,Dimension *width_out,Dimension *height_out)
{
	Dimension width;
	Dimension height;

    assert( _XfeIsAlive(w) );

	if (XfeIsPrimitive(w))
	{
		width  = _XfePreferredWidth(w);
		height = _XfePreferredHeight(w);
	}
	else if (XfeIsManager(w))
	{
		width  = _XfemPreferredWidth(w);
		height = _XfemPreferredHeight(w);
	}
	else
	{
		XtWidgetGeometry reply;

		XtQueryGeometry(w,NULL,&reply);

		if (reply.request_mode & CWWidth)
		{
			width = reply.width;
		}
		else
		{
			width = _XfeWidth(w);
		}


		if (reply.request_mode & CWHeight)
		{
			height = reply.height;
		}
		else
		{
			height = _XfeHeight(w);
		}
	}

	if (width_out)
	{
		*width_out = width;
	}

	if (height_out)
	{
		*height_out = height;
	}
}
/*----------------------------------------------------------------------*/

/*
 * This function should be really clever and deal with all the 
 * craziness of Xt geometry management.  This will probably happen
 * on a case by case basis.  As it stands, it works with the xfe
 * widgets, but I suspect it will have to be amended for more
 * sophisticated geometry magic.  
 *
 * The basic assumption here is that geometry requests always 
 * return YES as is in _XfeLiberalGeometryManager().
 */
/* extern */ Boolean
_XfeMakeGeometryRequest(Widget w,Dimension width,Dimension height)
{
	XtGeometryResult	request_result;
	XtWidgetGeometry	request;
	Boolean				result = False;

	assert( _XfeIsAlive(w) );
	
	request.request_mode = 0;

	/* Request a width change */
	if (width != _XfeWidth(w))
	{
		request.width = width;
		request.request_mode |= CWWidth;
	}

	/* Request a height change */
	if (height != _XfeHeight(w))
	{
		request.height = height;
		request.request_mode |= CWHeight;
	}

	/* WTF */
	if (request.request_mode == 0)
	{
		return False;
	}
	
	/* Make the request. */
	request_result = _XmMakeGeometryRequest(w,&request);
	
	/* Adjust geometry accordingly */
	if (request_result == XtGeometryYes)
	{
		result = True;
	}
	else if(request_result == XtGeometryNo)
	{
		result = False;
	}
	else if(request_result == XtGeometryAlmost)
	{
		result = False;
	}

	return result;
}
/*----------------------------------------------------------------------*/
/* extern */ XtGeometryResult
_XfeLiberalGeometryManager(Widget				child,
						   XtWidgetGeometry *	request,
						   XtWidgetGeometry *	reply)
{
	Widget w = XtParent(child);

	assert( XfeIsManager(w) );

	if (request->request_mode & XtCWQueryOnly)
	{
		return XtGeometryYes;
	}
	
	if (request->request_mode & CWX)
	{
		_XfeX(child) = request->x;
	}
	if (request->request_mode & CWY)
	{
		_XfeY(child) = request->y;
	}
	if (request->request_mode & CWWidth)
	{
		_XfeWidth(child) = request->width;
	}
	if (request->request_mode & CWHeight)
	{
		_XfeHeight(child) = request->height;
	}
	if (request->request_mode & CWBorderWidth)
	{
		_XfeBorderWidth(child) = request->border_width;
	}

	/*
	 * This should be a lot more clever.  We dont want to always
	 * re layout the children.  It works for now, but it could be 
	 * a lot more efficient. -re
	 */

	/* Layout the components */
	_XfeManagerLayoutComponents(w);
	
	/* Layout the children */
	_XfeManagerLayoutChildren(w);

	return XtGeometryYes;
}
/*----------------------------------------------------------------------*/
