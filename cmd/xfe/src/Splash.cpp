/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* 
   Splash.cpp -- class for dealing with the unix splash screen.
   Created: Chris Toshok <toshok@netscape.com>, 21-Dec-1996.
 */



#include "Splash.h"
#include "DisplayFactory.h"
#include "xpassert.h"
#include "prefapi.h"
#include <Xm/MenuShell.h>
#include <Xm/RowColumn.h>
#include <Xm/Label.h>

#ifdef NSPR20
#include "private/prpriv.h" /* For PR_NewNamedMonitor */
#else
#define PR_INTERVAL_NO_TIMEOUT	LL_MAXINT
#endif

#ifdef DEBUG_username
#define D(x) x
#else
#define D(x)
#endif

struct XFE_SplashEvent {
	PREvent	event;	/* the PREvent structure */
	char		*text;  /* the new text to be displayed. */
	XFE_Splash	*splash; /* the splash screen object we're going to muck with. */
};

static XFE_Splash *xfe_splash = NULL;

extern fe_icon_data Splash;
extern "C" Colormap fe_getColormap(fe_colormap *colormap);

fe_icon XFE_Splash::splash_icon = { 0 };

XFE_Splash::XFE_Splash(Widget toplevel)
{
  Arg av[10];
  int ac;
  fe_colormap *cmap = XFE_DisplayFactory::theFactory()->getSharedColormap();

  m_exposemonitor = PR_NewNamedMonitor("expose-monitor");
  m_eventmonitor = PR_NewNamedMonitor("event-monitor");
  m_stopmonitor = PR_NewNamedMonitor("stop-monitor");
  m_done = 0;

  m_context = XP_NewContext();
  m_context->fe.data = XP_NEW_ZAP(fe_ContextData);

#ifdef NSPR20
  m_thread = PR_CreateThread(PR_USER_THREAD, splashThreadProc, this,
							 PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD,
							 0);		/* default stack size */
#else
  m_thread = PR_CreateThread("splash-thread", 24, 0);
#endif
  m_eventqueue = PR_CreateEventQueue("splash-events", m_thread);

  ac = 0;
  XtSetArg(av[ac], XmNwidth, 1); ac++;
  XtSetArg(av[ac], XmNheight, 1); ac++;
  XtSetArg(av[ac], XmNcolormap, fe_getColormap(cmap)); ac++;
  XtSetArg(av[ac], XmNvisual, XFE_DisplayFactory::theFactory()->getVisual()); ac++;
  XtSetArg(av[ac], XmNdepth, XFE_DisplayFactory::theFactory()->getVisualDepth()); ac++;

  m_shell = XtCreatePopupShell("splashShell",
			       xmMenuShellWidgetClass,
			       toplevel,
			       av, ac);

  m_rc = XtVaCreateManagedWidget("splashRC",
				 xmRowColumnWidgetClass,
				 m_shell,
				 XmNspacing, 0,
				 NULL);

  CONTEXT_WIDGET(m_context) = m_shell;
  CONTEXT_DATA (m_context)->colormap = cmap;

  setBaseWidget(m_shell);

  XtGetApplicationResources (m_widget,
			     (XtPointer) CONTEXT_DATA (m_context),
			     fe_Resources, fe_ResourcesSize,
			     0, 0);

  fe_InitIconColors(m_context);

  fe_init_image_callbacks(m_context);

  fe_InitColormap (m_context);

  PREF_AlterSplashIcon(&Splash);

  fe_NewMakeIcon(m_shell,
		 BlackPixelOfScreen(XtScreen(m_shell)), //XXX hack... doesn't really matter though.
		 BlackPixelOfScreen(XtScreen(m_shell)), //XXX hack... doesn't really matter though.
		 &splash_icon,
		 NULL,
		 Splash.width,
		 Splash.height,
		 Splash.mono_bits, Splash.color_bits, Splash.mask_bits,
		 FALSE);
  
	XP_ASSERT(splash_icon.pixmap);

	m_splashlabel = XtVaCreateManagedWidget("splashPixmap",
											xmLabelWidgetClass,
											m_rc,
											XmNlabelType, XmPIXMAP,
											XmNlabelPixmap, splash_icon.pixmap,
											NULL);
	m_statuslabel = XtVaCreateManagedWidget("splashStatus",
											xmLabelWidgetClass,
											m_rc,
											NULL);

	XtAddEventHandler(m_splashlabel, ExposureMask, False, splashExpose_eh, this);

#ifndef NSPR20
	PR_Start(m_thread, splashThreadProc, this, NULL);
#endif
}

XFE_Splash::~XFE_Splash()
{
	if(m_eventqueue)
		PR_DestroyEventQueue(m_eventqueue);
	if (m_eventmonitor)
		PR_DestroyMonitor(m_eventmonitor);

	fe_DisposeColormap(m_context);
	XP_FREE(CONTEXT_DATA(m_context));
	XP_FREE(m_context);
}

void
XFE_Splash::splashExpose()
{
	PR_EnterMonitor(m_exposemonitor);
	PR_Notify(m_exposemonitor);
	PR_ExitMonitor(m_exposemonitor);

	XtRemoveEventHandler(m_splashlabel, ExposureMask, False, splashExpose_eh, this);
}

void
XFE_Splash::splashExpose_eh(Widget, XtPointer closure, XEvent *, Boolean *)
{
	XFE_Splash *splash = (XFE_Splash*)closure;

	splash->splashExpose();
}

/* Caller must wrap calls to show() in PR_XLock and PR_XUnlock. */
void
XFE_Splash::show()
{
	Dimension height_of_splash, width_of_splash;
	Dimension height_of_screen = HeightOfScreen(XtScreen(m_shell)),
		width_of_screen = WidthOfScreen(XtScreen(m_shell));

	XtRealizeWidget(m_shell);

	XtVaGetValues(m_shell,
				  XmNwidth, &width_of_splash,
				  XmNheight, &height_of_splash,
				  NULL);

	XtVaSetValues(m_shell,
				  XmNx, width_of_screen / 2 - width_of_splash / 2,
				  XmNy, height_of_screen / 2 - height_of_splash / 2,
				  NULL);

	XtPopup(m_shell, XtGrabNone);

	XSync(XtDisplay(m_shell), False);
}

/* Caller must wrap calls to hide() in PR_XLock and PR_XUnlock. */
void
XFE_Splash::hide()
{
	XtPopdown(m_shell);

	XSync(XtDisplay(m_shell), False);
}

void
XFE_Splash::waitForExpose()
{
	PR_EnterMonitor(m_exposemonitor);
	PR_Wait(m_exposemonitor, PR_INTERVAL_NO_TIMEOUT);
	PR_ExitMonitor(m_exposemonitor);

	PR_DestroyMonitor(m_exposemonitor);
}

void
XFE_Splash::setStatus(char *text)
{
	XmString xmstr = XmStringCreate(text, XmFONTLIST_DEFAULT_TAG);

	XtVaSetValues(m_statuslabel,
				  XmNlabelString, xmstr,
				  NULL);

	XmStringFree(xmstr);

	XSync(XtDisplay(m_statuslabel), False);

	PR_EnterMonitor(m_eventmonitor);
	PR_Notify(m_eventmonitor);
	PR_ExitMonitor(m_eventmonitor);
}

void
XFE_Splash::update_text_handler(XFE_SplashEvent *event)
{
	XP_ASSERT(event->text);

	event->splash->setStatus(event->text);
}

void
XFE_Splash::update_text_destructor(XFE_SplashEvent *event)
{
	XP_FREE(event);
}

PRThread *
XFE_Splash::getThread()
{
	return m_thread;
}

PREventQueue *
XFE_Splash::getEventQueue()
{
	return m_eventqueue;
}

PRMonitor *
XFE_Splash::getEventMonitor()
{
	return m_eventmonitor;
}

PRMonitor *
XFE_Splash::getStopMonitor()
{
	return m_stopmonitor;
}

void
XFE_Splash::splashThreadProc()
{
	PREvent      * event;

	for (;;) 
		{
			PR_EnterMonitor(m_stopmonitor);
			if (m_done)
				{
					PR_ExitMonitor(m_stopmonitor);

					return;
				}
			else
				{
					PR_ExitMonitor(m_stopmonitor);
				}

			PR_XLock();

			XtInputMask pending = XtAppPending(fe_XtAppContext);

			/* if there was a pending X event, handle it now */
			if (pending)
				XtAppProcessEvent(fe_XtAppContext, pending);
      
			PR_EnterMonitor(m_eventmonitor);
			event = PR_GetEvent(m_eventqueue);
			PR_ExitMonitor(m_eventmonitor);
      
			/* if we got an nspr event (telling us to update our status) do it */
			if (event)
				{
					PR_HandleEvent(event);
					PR_DestroyEvent(event);
				}
      
			PR_XUnlock();
		}
}

void
#ifndef NSPR20
XFE_Splash::splashThreadProc(void *a, void *)
#else
XFE_Splash::splashThreadProc(void *a)
#endif /* NSPR20 */
{
	XFE_Splash *splash = (XFE_Splash*)a;
	PRMonitor *monitor = splash->getStopMonitor();

	splash->splashThreadProc();

	delete splash;

	PR_EnterMonitor(monitor);
	PR_Notify(monitor);
	PR_ExitMonitor(monitor);

	D(printf ("exiting the splash thread\n");)

#ifndef NSPR20
	PR_Exit();
#endif
}

void
fe_splashStart(Widget toplevel)
{
	XP_ASSERT(!xfe_splash);

	xfe_splash = new XFE_Splash(toplevel);

	PR_XLock();

	xfe_splash->show();

	PR_XUnlock();

	xfe_splash->waitForExpose();
}

void
fe_splashUpdateText(char *text)
{
	XP_ASSERT(xfe_splash);
  
	D(printf ("fe_splashUpdateText('%s')\n", text);)

	PRMonitor *monitor = xfe_splash->getEventMonitor();
	XFE_SplashEvent *event = XP_NEW_ZAP(XFE_SplashEvent);

	PR_InitEvent(&event->event, NULL,
				 (PRHandleEventProc)XFE_Splash::update_text_handler,
				 (PRDestroyEventProc)XFE_Splash::update_text_destructor);

	event->text = text;
	event->splash = xfe_splash;
	PR_PostEvent(xfe_splash->getEventQueue(), &event->event);
	if (monitor)
    {
		/* wake up the processing routine */
		PR_EnterMonitor(monitor);
		PR_Notify(monitor);
		PR_ExitMonitor(monitor);

		/* now we wait until it actually happens. */
		PR_EnterMonitor(monitor);
		PR_Wait(monitor, PR_INTERVAL_NO_TIMEOUT);
		PR_ExitMonitor(monitor);
    }
}

void
fe_splashStop(void)
{
	XP_ASSERT(xfe_splash);
  
	D(printf ("fe_splashStop()\n");)

	PRMonitor *monitor = xfe_splash->getStopMonitor();

	XP_ASSERT(monitor);

	/* first we tell the splash screen to stop */
	PR_EnterMonitor(monitor);
	xfe_splash->m_done = 1;
	PR_ExitMonitor(monitor);

	/* now we wait until it actually happens. */
	PR_EnterMonitor(monitor);
	PR_Wait(monitor, PR_INTERVAL_NO_TIMEOUT);
	PR_ExitMonitor(monitor);

	PR_DestroyMonitor(monitor);
}
