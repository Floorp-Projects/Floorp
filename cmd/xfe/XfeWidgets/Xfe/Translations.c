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
/* Name:		<Xfe/Translations.c>									*/
/* Description:	Xfe widgets default and extra translations.				*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/XfeP.h>

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeArrow translations												*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ char _XfeArrowDefaultTranslations[] ="\
<Key>space:					ArmAndActivate()\n\
c<Btn1Down>:				Focus()\n\
<Btn1Down>:					Arm()\n\
<Btn1Up>:					Activate() Disarm()\n\
~c ~s ~a ~m<Key>Return:		ArmAndActivate()\n\
~c ~s ~a ~m<Key>space:		ArmAndActivate()\n\
:<Key>osfHelp:				Help()\n\
<Enter>:					Enter()\n\
<Leave>:					Leave()";

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeLabel translations												*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ char _XfeLabelDefaultTranslations[] ="\
<Btn1Down>:					Btn1Down()\n\
c<Btn1Down>:				Focus()\n\
:<Key>osfHelp:				Help()\n\
<Enter>:					Enter()\n\
<Leave>:					Leave()";

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeButton translations												*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ char _XfeButtonDefaultTranslations[] ="\
<Key>space:					ArmAndActivate()\n\
c<Btn1Down>:				Focus()\n\
<Btn1Down>:					Arm()\n\
<Btn1Up>:					Activate() Disarm()\n\
<Btn3Down>:					Btn3Down()\n\
<Btn3Up>:					Btn3Up()\n\
~c ~s ~a ~m<Key>Return:		ArmAndActivate()\n\
~c ~s ~a ~m<Key>space:		ArmAndActivate()\n\
:<Key>osfHelp:				Help()\n\
<Enter>:					Enter()\n\
<Leave>:					Leave()";

/* extern */ char _XfeButtonMotionAddTranslations[] ="\
<Motion>:					Motion()";

/* extern */ char _XfeButtonMotionRemoveTranslations[] ="\
<Motion>:					";

/*----------------------------------------------------------------------*/
/*																		*/
/* XfePrimitive translations											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ char _XfePrimitiveDefaultTranslations[] ="\
c<Btn1Down>:				Focus()\n\
:<Key>osfHelp:				Help()\n\
<Enter>:					Enter()\n\
<Leave>:					Leave()";

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBypassShell translations											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ char _XfeBypassShellDefaultTranslations[] ="\
<BtnUp>:					BtnUp()\n\
<BtnDown>:					BtnDown()";

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeButton extra translations											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ char _XfeBmButtonExtraTranslations[] ="\
<Btn1Motion>:				Motion()\n\
<Motion>:					Motion()";

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeToolBar extra translations										*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ char _XfeToolBarExtraTranslations[] ="\
<Btn3Down>:					Btn3Down()\n\
<Btn3Up>:					Btn3Up()";

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBox translations.											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ char _XfeComboBoxTextEditableTranslations[] ="\
~c s ~m ~a <Btn1Down>:			extend-start()\n\
c ~s ~m ~a <Btn1Down>:			move-destination()\n\
~c ~s ~m ~a <Btn1Down>:			grab-focus()\n\
~c ~m ~a <Btn1Motion>:			extend-adjust()\n\
~c ~m ~a <Btn1Up>:				extend-end()";

/* extern */ char _XfeComboBoxTextReadOnlyTranslations[] ="\
~c ~s ~m ~a <Btn1Down>:			grab-focus() Popup()\n\
~c ~m ~a <Btn1Motion>:			\n\
~c ~m ~a <Btn1Up>:				Popdown()";

#if 0
/* extern */ char _XfeComboBoxArrowTranslations[] ="\
~c ~s ~m ~a <Btn1Down>:			Highlight() Arm() Popup()\n\
~c ~m ~a <Btn1Up>:				Activate() Disarm() Popdown()";
#else
/* extern */ char _XfeComboBoxArrowTranslations[] ="\
~c ~s ~m ~a <Btn1Down>:			Highlight() Arm()  Popup()\n\
~c ~m ~a <Btn1Up>:				Activate() Disarm() Popdown()";
#endif

/* extern */ char _XfeComboBoxExtraTranslations[] ="\
~c ~s ~m ~a <Btn1Down>:			Highlight() Popup()\n\
~c ~m ~a <Btn1Up>:				Popdown()";

/*----------------------------------------------------------------------*/
/*																		*/
/* Translation / Action functions										*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeOverrideTranslations(Widget w,String table)
/*----------------------------------------------------------------------*/
{
	XtTranslations parsed = NULL;

	assert( _XfeIsAlive(w) );
	assert( table != NULL );
	
	parsed = XtParseTranslationTable(table);
	
	XtOverrideTranslations(w,parsed);
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeAddActions(Widget w,XtActionList actions,Cardinal num_actions)
/*----------------------------------------------------------------------*/
{
	assert( _XfeIsAlive(w) );
	assert( actions != NULL );
	assert( num_actions > 0 );

	XtAppAddActions(XtWidgetToApplicationContext(w),actions,num_actions);
}
/*----------------------------------------------------------------------*/
