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
/* Name:		<Xfe/Find.c>											*/
/* Description:	Xfe widgets utilities to find children.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <stdio.h>
#include <stdlib.h>

#include <Xfe/XfeP.h>
#include <Xfe/PrimitiveP.h>
#include <Xfe/ManagerP.h>

/*----------------------------------------------------------------------*/
/*																		*/
/* Private find functions												*/
/*																		*/
/*----------------------------------------------------------------------*/
static Widget	FindThisLevel	(WidgetList			children,
								 Cardinal			num_children,
								 XfeWidgetTestFunc	func,
								 int				mask,
								 XtPointer			data);
/*----------------------------------------------------------------------*/
static Widget	FindNextLevel	(WidgetList			children,
								 Cardinal			num_children,
								 XfeWidgetTestFunc	func,
								 int				mask,
								 Boolean			popups,
								 XtPointer			data);
/*----------------------------------------------------------------------*/
static Boolean	FindTestName	(Widget w,XtPointer data);
static Boolean	FindTestClass	(Widget w,XtPointer data);
static Boolean	FindTestWindow	(Widget w,XtPointer data);
static Boolean	FindTestCoord	(Widget w,XtPointer data);
/*----------------------------------------------------------------------*/
#define FLAG_ALIVE(w,m)		(((m) & XfeFIND_ALIVE) ? _XfeIsAlive(w) : True)
#define FLAG_REALIZED(w,m)	(((m) & XfeFIND_REALIZED) ? XtIsRealized(w) : True)
#define FLAG_MANAGED(w,m)	(((m) & XfeFIND_MANAGED) ? XtIsManaged(w) : True)
/*----------------------------------------------------------------------*/



/*----------------------------------------------------------------------*/
/*																		*/
/* Descendant find functions											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeDescendantFindByFunction(Widget				w,
							XfeWidgetTestFunc	func,
							int					mask,
							Boolean				popups,
							XtPointer			data)
{
	assert( w != NULL );
	assert( func != NULL );

	if (!w || !func)
	{
		return NULL;
	}

	/* Look at the normal children */
	if (XtIsComposite(w))
	{
		WidgetList	children = _XfemChildren(w);
		Cardinal	num_children = _XfemNumChildren(w);
		Widget		test;

		test = FindThisLevel(children,num_children,func,mask,data);

		if (!test)
		{
			test = FindNextLevel(children,num_children,func,mask,popups,data);
		}

		if (test)
		{
			return test;
		}
	}

	/* Look at the popup children */
	if (XtIsWidget(w) && popups)
	{
		WidgetList	children = _XfePopupList(w);
		Cardinal	num_children = _XfeNumPopups(w);
		Widget		test;

		test = FindThisLevel(children,num_children,func,mask,data);

		if (!test)
		{
			test = FindNextLevel(children,num_children,func,mask,popups,data);
		}

		if (test)
		{
			return test;
		}
	}

	return NULL;
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeDescendantFindByName(Widget w,String name,int mask,Boolean popups)
{
	return XfeDescendantFindByFunction(w,
									   FindTestName,
									   mask,
									   popups,
									   (XtPointer) name);
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeDescendantFindByClass(Widget w,WidgetClass wc,int mask,Boolean popups)
{
	return XfeDescendantFindByFunction(w,
									   FindTestClass,
									   mask,
									   popups,
									   (XtPointer) wc);
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeDescendantFindByWindow(Widget w,Window window,int mask,Boolean popups)
{
	return XfeDescendantFindByFunction(w,
									   FindTestWindow,
									   mask & XfeFIND_REALIZED,
									   popups,
									   (XtPointer) window);
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeDescendantFindByCoordinates(Widget w,Position x,Position y)
{
	XPoint p;

	p.x = x;
	p.y = y;

	return XfeDescendantFindByFunction(w,FindTestCoord,XfeFIND_ALL,
									   False,(XtPointer) &p);
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeChildFindByIndex(Widget w,Cardinal i)
{
	Widget result = NULL;

    assert( _XfeIsAlive(w) );
    assert( XtIsComposite(w) );

	if ((i < _XfemNumChildren(w)) && _XfemChildren(w))
	{
		result = _XfemChildren(w)[i];
	}
	
	return result;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Find ancestor functions.												*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeAncestorFindByFunction(Widget				w,
						  XfeWidgetTestFunc		func,
						  int					mask,
						  XtPointer				data)
{
	Widget test = w;

	assert( w != NULL );
	assert( func != NULL );

	if (!w || !func)
	{
		return NULL;
	}

	while(test)
	{
		Boolean flag_alive = FLAG_ALIVE(test,mask);
		Boolean flag_realized = (flag_alive && FLAG_REALIZED(test,mask));
		Boolean flag_managed = (flag_realized && FLAG_MANAGED(test,mask));

		if (flag_alive && 
			flag_realized && 
			flag_managed && 
			(*func)(test,data))
		{
			return test;
		}
		
		test = _XfeParent(test);
	}
	
	return NULL;
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeAncestorFindByName(Widget w,String name,int mask)
{
	return XfeAncestorFindByFunction(w,
									 FindTestName,
									 mask,
									 (XtPointer) name);
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeAncestorFindByClass(Widget w,WidgetClass wc,int mask)
{
	return XfeAncestorFindByFunction(w,
									 FindTestClass,
									 mask,
									 (XtPointer) wc);
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeAncestorFindByWindow(Widget w,Window window,int mask)
{
	return XfeAncestorFindByFunction(w,
									 FindTestWindow,
									 mask & XfeFIND_REALIZED,
									 (XtPointer) window);
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeAncestorFindTopLevelShell(Widget w)
{
    return XfeAncestorFindByClass(w,topLevelShellWidgetClass,XfeFIND_ANY);
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeAncestorFindApplicationShell(Widget w)
{
    return XfeAncestorFindByClass(w,applicationShellWidgetClass,XfeFIND_ANY);
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Private find functions												*/
/*																		*/
/*----------------------------------------------------------------------*/
static Widget
FindThisLevel(WidgetList			children,
			  Cardinal				num_children,
			  XfeWidgetTestFunc		func,
			  int					mask,
			  XtPointer				data)
{
	if (children && num_children)
	{
		Cardinal	i;
		Widget *	pw;
		
		/* Try to find a match in the current level first */
		for (i = 0,pw = children; i < num_children; i++,pw++)
		{
			Boolean flag_alive = False;
			Boolean flag_managed = False;
			Boolean flag_realized = False;

			flag_alive = FLAG_ALIVE(*pw,mask);
			flag_realized = (flag_alive && FLAG_REALIZED(*pw,mask));
			flag_managed = (flag_realized && FLAG_MANAGED(*pw,mask));
				
			if (flag_alive && 
				flag_realized && 
				flag_managed && 
				(*func)(*pw,data))
			{
				return *pw;
			}
		}
	}

	return NULL;
}
/*----------------------------------------------------------------------*/
static Widget
FindNextLevel(WidgetList			children,
			  Cardinal				num_children,
			  XfeWidgetTestFunc		func,
			  int					mask,
			  Boolean				popups,
			  XtPointer				data)
{
	if (children && num_children)
	{
		Cardinal	i;
		Widget *	pw;
		Widget		test;
		
		/* Recurse into the next level */
		for(i = 0,pw = children; i < num_children; i++,pw++)
		{
			test = XfeDescendantFindByFunction(*pw,func,mask,popups,data);

			if (test)
			{
				return test;
			}
		}
	}

	return NULL;
}
/*----------------------------------------------------------------------*/
static Boolean
FindTestName(Widget w,XtPointer data)
{
	String name = (String) data;

	if (w && name && *name && (strcmp(XtName(w),name) == 0))
	{
		return True;
	}

	return False;
}
/*----------------------------------------------------------------------*/
static Boolean
FindTestClass(Widget w,XtPointer data)
{
	WidgetClass wc = (WidgetClass) data;

	if (w && wc && XtIsSubclass(w,wc))
	{
		return True;
	}

	return False;
}
/*----------------------------------------------------------------------*/
static Boolean
FindTestWindow(Widget w,XtPointer data)
{
	Window window = (Window) data;

	if (w && window && (_XfeWindow(w) == window))
	{
		return True;
	}

	return False;
}
/*----------------------------------------------------------------------*/
static Boolean
FindTestCoord(Widget w,XtPointer data)
{
	XPoint * p = (XPoint *) data;

	if ((p->x >= _XfeX(w)) && 
		(p->x <= _XfeX(w) + _XfeWidth(w)) &&
		(p->y >= _XfeY(w)) && 
		(p->y <= _XfeY(w) + _XfeHeight(w)))
	{
		return True;
	}

	return False;
}
/*----------------------------------------------------------------------*/

