/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		<Xfe/StringUtil.c>										*/
/* Description:	Xfe widgets XmString utilities.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#include <Xfe/XfeP.h>

#include <Xm/LabelP.h>
#include <Xm/LabelGP.h>
#include <Xfe/LabelP.h>

/*----------------------------------------------------------------------*/
/*																		*/
/* This function is useful when effecient access to a widget's 			*/
/* XmNlabelString is needed.  It avoids the GetValues() and 			*/
/* XmStringCopy() overhead.  Of course, the result should be 			*/
/* considered read only.												*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ XmString
XfeFastAccessLabelString(Widget w)
{
	XmString	label_string = NULL;

	assert( _XfeIsAlive(w) );

	if (!_XfeIsAlive(w))
	{
		return NULL;
	}

	if (XfeIsLabel(w))
	{
		label_string = ((XfeLabelWidget) w) -> xfe_label . label_string;
	}
#ifdef DEBUG_ramiro
	else
	{
		assert( 0 );
	}
#endif

	return label_string;
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* XmString utils														*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ XmString
XfeXmStringCopy(Widget w,XmString xmstr,String fallback)
{
    XmString new_xmstr;
    
    /* Make sure the string is setup properly */
    if (!xmstr)
    {
		/* If no xmstring is given, create using the fallback cstring */
		new_xmstr = XmStringCreateLocalized(fallback);
    }
    else
    {
		/* Otherwise make a carbon copy - no check done to verify xmstring */
		new_xmstr = XmStringCopy(xmstr);
    }
    
    return new_xmstr;
}
/*----------------------------------------------------------------------*/
/* extern */ String
XfeXmStringGetPSZ(XmString xmstr,char * tag)
{
	String		psz_string = NULL;

	if (xmstr)
	{
		XmStringGetLtoR(xmstr,tag,&psz_string);
	}	
	
	return psz_string;
}
/*----------------------------------------------------------------------*/
/*extern*/ void
XfeSetXmStringPSZ(Widget w,String name,char * tag,char * value)
{
	XmString xmstr;

	assert( w != NULL );
	assert( name != NULL );

	xmstr = XmStringCreateLtoR(value,tag);

    XfeSetValue(w,name,(XtArgVal) xmstr);

	if (xmstr)
	{
		XmStringFree(xmstr);
	}
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XmStringTable utils													*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ XmString *
XfeXmStringTableCopy(XmString * items,Cardinal num_items)
{
	XmString * copy = NULL;

	return copy;
}
/*----------------------------------------------------------------------*/

