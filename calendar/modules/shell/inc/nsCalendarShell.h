/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _nsCalendarShell_h__
#define _nsCalendarShell_h__

#include <stdio.h>
#include "nsICalendarShell.h"
#include "nsIFactory.h"
#include "nsRepository.h"
#include "nsIShellInstance.h"
#include "nsIWidget.h"
#include "nsGUIEvent.h"
#include "nsRect.h"
#include "nsApplicationManager.h"
#include "nsIContentViewer.h"
#include "nsCalendarContainer.h"
#include "nsIXPFCObserverManager.h"
#include "nsCalUICIID.h"
#include "nsCalUtilCIID.h"
#include "nsCRT.h"
#include "plstr.h"
#include "julnstr.h"
#include "nsICommandServer.h"
#include "nsCalList.h"
#include "nsCalUserList.h"
#include "nsCalSessionMgr.h"
#include "capi.h"

/*
 * CalendarShell Class Declaration
 */
class nsCalLoggedInUser;

class nsCalendarShell : public nsICalendarShell {
public:
  nsCalendarShell();
  ~nsCalendarShell();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();

  NS_METHOD Logon();
  NS_METHOD Logoff();
  NS_METHOD LoadUI();
  NS_METHOD LoadPreferences();
  NS_METHOD ParseCommandLine();

  NS_METHOD EnsureUserPath( JulianString& sPath );

  NS_IMETHOD SetCAPISession(CAPISession aCAPISession);
  NS_IMETHOD_(CAPISession) GetCAPISession();

  NS_IMETHOD SetCAPIHandle(CAPIHandle aCAPIHandle);
  NS_IMETHOD_(CAPIHandle) GetCAPIHandle();

  NS_IMETHOD SetCAPIPassword(char * aPassword) ;
  NS_IMETHOD_(char *) GetCAPIPassword() ;

  // nsIAppShell interfaces
  NS_IMETHOD Create(int* argc, char ** argv) ;
  NS_IMETHOD SetDispatchListener(nsDispatchListener* aDispatchListener) ;
  NS_IMETHOD Exit();
  virtual nsresult Run();
  virtual void* GetNativeData(PRUint32 aDataType) ;

  NS_IMETHOD_(nsEventStatus) HandleEvent(nsGUIEvent *aEvent)  ;
  NS_IMETHOD GetWebViewerContainer(nsIWebViewerContainer ** aWebViewerContainer) ;

  NS_IMETHOD StartCommandServer();
  NS_IMETHOD ReceiveCommand(nsString& aCommand, nsString& aReply);
  NS_IMETHOD InitFactoryObjs();

private:
  NS_METHOD InitialLoadData();
  NS_METHOD RegisterFactories();
  NS_METHOD SetDefaultPreferences();
  NS_METHOD EnvVarsToValues(JulianString& s);

private:
  nsIXPFCObserverManager * mObserverManager;

// XXX Should be private
public:
  nsIShellInstance * mShellInstance ;
  nsICalendarContainer * mDocumentContainer ;

  CAPISession mCAPISession;
  CAPIHandle mCAPIHandle;
  nsICalendarUser* mpLoggedInUser;

  JulianString msCalURL;  /* the calendar associated with this user */
  char * mCAPIPassword;   /* the password which must be entered by the user */

  nsCalUserList mUserList;
  nsCalList mCalList;
  nsCalSessionMgr mSessionMgr;
  nsICommandServer * mCommandServer;

};

nsEventStatus PR_CALLBACK HandleEventApplication(nsGUIEvent *aEvent) ;

#endif
