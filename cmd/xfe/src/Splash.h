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
   Splash.h -- class for dealing with the unix splash screen.
   Created: Chris Toshok <toshok@netscape.com>, 21-Dec-1996.
 */



#ifndef _xfe_splash_h
#define _xfe_splash_h

#include "structs.h"
#include "xp_core.h"
#include "Component.h"
#include "prmon.h"
#include "prthread.h"
#ifndef NSPR20
#include "prevent.h"
#else
#include "plevent.h"
#endif
#include "xfe.h"
#include "icons.h"

typedef struct XFE_SplashEvent XFE_SplashEvent;

class XFE_Splash : public XFE_Component
{
public:
  XFE_Splash(Widget toplevel);
  virtual ~XFE_Splash();

  virtual void show();
  virtual void hide();

  void waitForExpose();

  PRMonitor *getEventMonitor();
  PRMonitor *getStopMonitor();
  PREventQueue *getEventQueue();
  PRThread *getThread();

  void setStatus(char *text);

  static void update_text_handler(XFE_SplashEvent *event);
  static void update_text_destructor(XFE_SplashEvent *event);

  int m_done;

private:
  Widget m_shell;
  Widget m_rc;
  Widget m_splashlabel;
  Widget m_statuslabel;
  MWContext *m_context;

  static fe_icon splash_icon;

  PRThread *m_thread;
  PREventQueue *m_eventqueue;
  PRMonitor *m_eventmonitor;
  PRMonitor *m_exposemonitor;
  PRMonitor *m_stopmonitor;

  void splashExpose();
  static void splashExpose_eh(Widget, XtPointer, XEvent *, Boolean *);

  void splashThreadProc();
#ifndef NSPR20
  static void splashThreadProc(void *, void*);
#else
  static void splashThreadProc(void *);
#endif /* NSPR20 */
};


extern "C" void fe_splashStart(Widget toplevel);
extern "C" void fe_splashUpdateText(char *text);
extern "C" void fe_splashStop();

#endif /* _xfe_splash_h */
