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
/* Name:		<Xfe/Manager.h>											*/
/* Description:	XfeManager widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeManager_h_							/* start Manager.h		*/
#define _XfeManager_h_

#include <Xfe/Xfe.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager resource names											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmNchangeManagedCallback			"changeManagedCallback"
#define XmNlayoutCallback					"layoutCallback"

#define XmNcomponentChildren				"componentChildren"
#define XmNmaxComponentChildrenHeight		"maxComponentChildrenHeight"
#define XmNmaxComponentChildrenWidth		"maxComponentChildrenWidth"
#define XmNmaxStaticChildrenHeight			"maxStaticChildrenHeight"
#define XmNmaxStaticChildrenWidth			"maxStaticChildrenWidth"
#define XmNnumComponentChildren				"numComponentChildren"
#define XmNnumManagedComponentChildren		"numManagedComponentChildren"
#define XmNnumManagedStaticChildren			"numManagedStaticChildren"
#define XmNnumStaticChildren				"numStaticChildren"
#define XmNstaticChildren					"staticChildren"
#define XmNtotalComponentChildrenHeight		"totalComponentChildrenHeight"
#define XmNtotalComponentChildrenWidth		"totalComponentChildrenWidth"
#define XmNtotalStaticChildrenHeight		"totalStaticChildrenHeight"
#define XmNmanagerChildType				"managerChildType"
#define XmNignoreConfigure				"ignoreConfigure"
#define XmNtotalStaticChildrenWidth			"totalStaticChildrenWidth"
#define XmNbusy							"busy"
#define XmNbusyCursor					"busyCursor"
#define XmNbusyCursorOn					"busyCursorOn"

#define XmRManagerChildType				"ManagerChildType"
#define XmCBusy							"Busy"
#define XmCBusyCursor					"BusyCursor"
#define XmCIgnoreConfigure				"IgnoreConfigure"
#define XmCBusyCursorOn					"BusyCursorOn"
#define XmRLinkedChildren				"LinkedChildren"

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRManagerChildType													*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
	XmMANAGER_COMPONENT_INVALID,			/*						*/
	XmMANAGER_COMPONENT_CHILD,				/*						*/
	XmMANAGER_DYNAMIC_CHILD,				/*						*/
	XmMANAGER_STATIC_CHILD					/*						*/
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager children info/apply mask bits								*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeCHILDREN_INFO_NONE		(0)
#define XfeCHILDREN_INFO_ALIVE		(1 << 0)
#define XfeCHILDREN_INFO_MANAGED	(1 << 1)
#define XfeCHILDREN_INFO_REALIZED	(1 << 2)
#define XfeCHILDREN_INFO_ANY		(~(0))

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager class names												*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeManagerWidgetClass;

typedef struct _XfeManagerClassRec *	XfeManagerWidgetClass;
typedef struct _XfeManagerRec *			XfeManagerWidget;

#define XfeIsManager(w)	XtIsSubclass(w,xfeManagerWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* Manager apply function type											*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef void		(*XfeManagerApplyProc)	(Widget		w,
											 Widget		child,
											 XtPointer	client_data);

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager public methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeManagerLayout				(Widget					w);
/*----------------------------------------------------------------------*/
extern void
XfeManagerSetChildrenValues		(Widget					w,
								 ArgList				args,
								 Cardinal				n,
								 Boolean				only_managed);
/*----------------------------------------------------------------------*/
extern void
XfeManagerResizeChildren		(Widget					w,
								 Boolean				set_width,
								 Boolean				set_height,
								 Boolean				only_managed);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeManager public children apply functions							*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeManagerApply					(Widget					w,
								 int					mask,
								 XfeManagerApplyProc	proc,
								 XtPointer				data,
								 Boolean				private_components,
								 Boolean				freeze_layout);
/*----------------------------------------------------------------------*/
extern void
XfeManagerApplyLinked			(Widget					w,
								 unsigned char			child_type,
								 int					mask,
								 XfeManagerApplyProc	proc,
								 XtPointer				data,
								 Boolean				freeze_layout);
/*----------------------------------------------------------------------*/

XFE_END_CPLUSPLUS_PROTECTION

#endif						/* end Manager.h	*/
