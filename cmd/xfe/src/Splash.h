/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "plevent.h"
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

  static void splashThreadProc(void *);
};


extern "C" void fe_splashStart(Widget toplevel);
extern "C" void fe_splashUpdateText(char *text);
extern "C" void fe_splashStop();

#endif /* _xfe_splash_h */
