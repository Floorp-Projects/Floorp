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
/* Name:		<XfeTest/TestApp.c>										*/
/* Description:	Xfe widget tests stuff.									*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/XfeTest.h>

#include <X11/Xmu/Editres.h>
#include <X11/Xmu/Converters.h>

#include <X11/IntrinsicP.h>

static XtConvertArgRec parentCvtArg[] = 
{
	{
		XtWidgetBaseOffset,
		(XtPointer) XtOffsetOf(WidgetRec , core . parent),
		sizeof(Widget)
	}
};

/*----------------------------------------------------------------------*/
static XtAppContext		_xfe_app_context = NULL;
static Widget			_xfe_app_shell = NULL;
/*----------------------------------------------------------------------*/

extern char * fallback_resources[];

/*----------------------------------------------------------------------*/
/* extern */ void
XfeAppCreate(char * app_name,int * argc,String * argv)
{
	assert( _xfe_app_context == NULL );
	assert( _xfe_app_shell == NULL );
	
    _xfe_app_shell = XtAppInitialize(&_xfe_app_context,app_name,
									 NULL,0,argc,argv,
									 fallback_resources,
									 NULL,0);

	XtVaSetValues(_xfe_app_shell,
  				  XmNmappedWhenManaged,		False,
				  XmNx,						XfeScreenWidth(_xfe_app_shell)/2,
				  XmNy,						XfeScreenHeight(_xfe_app_shell)/2,
				  XmNwidth,					1,
				  XmNheight,				1,
				  NULL);

/* 	XfeAddEditresSupport(_xfe_app_shell); */
	XfeRegisterStringToWidgetCvt();
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeAppCreateSimple(char *		app_name,
				   int *		argc,
				   String *		argv,
				   char *		frame_name,
				   Widget *		frame_out,
				   Widget *		form_out)
{
	assert( _xfe_app_context == NULL );
	assert( _xfe_app_shell == NULL );
	assert( frame_out != NULL );
	assert( form_out != NULL );

	XfeAppCreate(app_name,argc,argv);

	*frame_out = XfeFrameCreate(frame_name,NULL,0);

    *form_out = XfeDescendantFindByName(*frame_out,
										"MainForm",
										XfeFIND_ANY,False);
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeFrameCreate(char * frame_name,ArgList args,Cardinal num_args)
{
	Widget frame;
	Widget main_form;

	assert( _xfe_app_context != NULL );
	assert( XfeIsAlive(_xfe_app_shell) );

	frame = XtVaCreatePopupShell(frame_name,
								 xfeFrameShellWidgetClass,
								 _xfe_app_shell,
								 NULL);
	
	main_form = XfeCreateManagedForm(frame,"MainForm",NULL,0);

  	XfeAddEditresSupport(frame);

	return frame;
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeFrameCreateWithChrome(char * frame_name,ArgList args,Cardinal num_args)
{
	Widget frame;
	Widget chrome;

	assert( _xfe_app_context != NULL );
	assert( XfeIsAlive(_xfe_app_shell) );

	frame = XtVaCreatePopupShell(frame_name,
								 xfeFrameShellWidgetClass,
								 _xfe_app_shell,
								 NULL);
	
    chrome = XtVaCreateManagedWidget("Chrome",
									 xfeChromeWidgetClass,
									 frame,
									 NULL);

  	XfeAddEditresSupport(frame);

	return frame;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeAddEditresSupport(Widget shell)
{
	assert( XfeIsAlive(shell) );
	assert( XtIsShell(shell) );

    XtAddEventHandler(shell,0,True,_XEditResCheckMessages,NULL);
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeRegisterStringToWidgetCvt(void)
{
	XtSetTypeConverter(XtRString,
					   XtRWidget,
					   XmuNewCvtStringToWidget,
					   parentCvtArg,
					   XtNumber(parentCvtArg),
					   XtCacheNone,
					   NULL);
}
/*----------------------------------------------------------------------*/
/* extern */ XtAppContext
XfeAppContext(void)
{
	assert( _xfe_app_context != NULL );

	return _xfe_app_context;
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeAppShell(void)
{
	assert( _xfe_app_shell != NULL );

	return _xfe_app_shell;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeAppMainLoop(void)
{
	assert( _xfe_app_context != NULL );

	XtRealizeWidget(_xfe_app_shell);

    XtAppMainLoop(_xfe_app_context);
}
/*----------------------------------------------------------------------*/
/* extern */ XtIntervalId
XfeAppAddTimeOut(unsigned long			interval,
				 XtTimerCallbackProc	proc,
				 XtPointer				client_data)
{
	return XtAppAddTimeOut(XfeAppContext(),interval,proc,client_data);
}
/*----------------------------------------------------------------------*/
