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
/* Name:		<Xfe/Converters.c>										*/
/* Description:	Xfe widgets resource converters source.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/XfeP.h>

#include <string.h> /* for strncasecmp() */

/*----------------------------------------------------------------------*/
/*																		*/
/* Converter Args for StringTo{misc}									*/
/*																		*/
/*----------------------------------------------------------------------*/
static XtConvertArgRec _XfeWidgetConvertArg[] = 
{
	{
		XtWidgetBaseOffset,
		(XtPointer) XtOffsetOf(WidgetRec , core . self),
		sizeof(Widget)
	}
};
/*----------------------------------------------------------------------*/

#define COMPARE(s,v) (strncasecmp( (s),(v),strlen(v) ) == 0)

/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeCvtStringToModifiers(Display *	dpy,
					   XrmValue *	args,
					   Cardinal *	nargs,
					   XrmValue *	from,
					   XrmValue *	to,
					   XtPointer *	data)
{
	String		str = (String) from->addr;
	Modifiers	mod = 0;
   
	if      (COMPARE(str,"no"))			mod = 0;
	else if (COMPARE(str,"any"))		mod = AnyModifier;
	else if (COMPARE(str,"alt"))		mod = Mod1Mask;
	else if (COMPARE(str,"control"))	mod = ControlMask;
	else if (COMPARE(str,"lock"))		mod = LockMask;
	else if (COMPARE(str,"mod1"))		mod = Mod1Mask;
	else if (COMPARE(str,"mod2"))		mod = Mod2Mask;
	else if (COMPARE(str,"mod3"))		mod = Mod3Mask;
	else if (COMPARE(str,"mod4"))		mod = Mod4Mask;
	else if (COMPARE(str,"mod5"))		mod = Mod4Mask;
	else if (COMPARE(str,"shift"))		mod = ShiftMask;
    else
	{
		XtDisplayStringConversionWarning(dpy,str,XmRModifiers);

		return False;
	}

	_XfeConverertDone(mod,Modifiers);
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeRegisterStringToModifiersCvt()
{
	XtSetTypeConverter(XmRString,
					   XmRModifiers,
					   XfeCvtStringToModifiers,
					   _XfeWidgetConvertArg,
					   XtNumber(_XfeWidgetConvertArg),
					   XtCacheNone,
					   NULL);
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeRegisterConverters()
{
	XfeRegisterStringToModifiersCvt();
}
/*----------------------------------------------------------------------*/
