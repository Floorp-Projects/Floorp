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

/*
 * Define while hooking up the XML parser...
 */

#define USE_PARSER 1
#define HARDCODE_UI

class nsICalendarShell;

#include <stdio.h>
#include "nsCalendarWidget.h"

#include "nsCalShellCIID.h"
#include "nscalcids.h"
#include "nsCRT.h"
#include "plstr.h"
#include "julnstr.h"
#include "nsICalTimeContext.h"
#include "nsICalComponent.h"
#include "nsIXPFCObserver.h"
#include "nsIXPFCObserverManager.h"
#include "nsIXPFCCanvasManager.h"
#include "nsIXPFCCommand.h"
#include "nsCalDurationCommand.h"
#include "nsCalDayListCommand.h"
#include "nsXPFCMethodInvokerCommand.h"
#include "nsXPFCActionCommand.h"
#include "nsIXPFCSubject.h"
#include "nsICalContextController.h"
#include "nsCalTimebarContextController.h"
#include "nsCalMonthContextController.h"
#include "nsIXPFCCanvas.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsIDTD.h"
#include "nsCalXMLContentSink.h"
#include "nsCalXMLDTD.h"
#include "nsXPFCXMLContentSink.h"
#include "nsXPFCXMLDTD.h"
#include "nsCalStreamListener.h"
#include "nsCurlParser.h"
#include "nsCalToolkit.h"
#include "nsBoxLayout.h"
#include "nsStreamManager.h"

#include "nsColor.h"
#include "nsFont.h"

#include "nsCalDayViewCanvas.h"
#include "nsCalMultiDayViewCanvas.h"
#include "nsCalTodoComponentCanvas.h"
#include "nsCalTimebarHeading.h"
#include "nsCalTimebarUserHeading.h"
#include "nsCalTimebarTimeHeading.h"

#include "nsCalStatusCanvas.h"
#include "nsCalCommandCanvas.h"

#include "nsICalendarShell.h"
#include "nsCalendarShell.h"

#include "plstr.h"
#include "prprf.h"  /* PR_snprintf(...) */
#include "prmem.h"  /* PR_Malloc(...) / PR_Free(...) */

// hardcode names of dll's
#ifdef NS_WIN32
  #define CALUI_DLL   "calui10.dll"
  #define UTIL_DLL    "util10.dll"
  #define PARSER_DLL  "calparser10.dll"
  #define XPFC_DLL    "xpfc10.dll"
#else
  #define CALUI_DLL  "libcalui10.so"
  #define UTIL_DLL   "libutil10.so"
  #define PARSER_DLL "libcalparser10.so"
  #define XPFC_DLL   "libxpfc10.so"
#endif

#define FOREGROUND_COLOR NS_RGB(0,0,128)

#define HARDCODE_SPACE 420

nsCalendarWidget::nsCalendarWidget()
{
  NS_INIT_REFCNT();
  mCalendarShell = nsnull;
  mWindow = nsnull;
}

NS_IMPL_QUERY_INTERFACE(nsCalendarWidget, kICalWidgetIID)
NS_IMPL_ADDREF(nsCalendarWidget)
NS_IMPL_RELEASE(nsCalendarWidget)

nsCalendarWidget::~nsCalendarWidget()
{
  // Release windows and views
  NS_IF_RELEASE(mRootCanvas);
  NS_IF_RELEASE(mWindow);
  ((nsIApplicationShell *)mCalendarShell)->Release();
}


nsresult nsCalendarWidget::Init(nsNativeWidget aNativeParent,
                                nsIDeviceContext* aDeviceContext,
                                nsIPref* aPrefs,
                                const nsRect& aBounds,
                                nsScrollPreference aScrolling)

{
  return NS_OK;
}

nsresult nsCalendarWidget::Init(nsIWidget * aParent,
                                const nsRect& aBounds,
                                nsICalendarShell * aCalendarShell)
{

  nsresult res ;

  mCalendarShell = aCalendarShell;

  NS_ADDREF(((nsIApplicationShell *)mCalendarShell));

  gXPFCToolkit->SetObserverManager(mCalendarShell->GetObserverManager());

  /*
   * Create the Root Canvas
   */

  static NS_DEFINE_IID(kIXPFCCanvasIID, NS_IXPFC_CANVAS_IID);


  res = nsRepository::CreateInstance(kCXPFCCanvasCID, 
                                     nsnull, 
                                     kCXPFCCanvasCID, 
                                     (void **)&mRootCanvas);

  if (NS_OK != res)
    return res ;

  gXPFCToolkit->GetCanvasManager()->SetRootCanvas(mRootCanvas);

  mRootCanvas->Init(aParent, aBounds,(((nsCalendarShell *)mCalendarShell)->mShellInstance->GetShellEventCallback()));

  return res ;
}

nsRect nsCalendarWidget::GetBounds()
{
  nsRect zr(0, 0, 0, 0);
  if (nsnull != mRootCanvas) {
    mRootCanvas->GetBounds(zr);
  }
  return zr;
}

void nsCalendarWidget::SetBounds(const nsRect& aBounds)
{
}

void nsCalendarWidget::Move(PRInt32 aX, PRInt32 aY)
{
}

void nsCalendarWidget::Show()
{
}

void nsCalendarWidget::Hide()
{
}

nsresult nsCalendarWidget::BindToDocument(nsISupports *aDoc, const char *aCommand)
{
  return NS_OK;
}

nsresult nsCalendarWidget::SetContainer(nsIContentViewerContainer* aContainer)
{
  return NS_OK;
}

nsresult nsCalendarWidget::GetContainer(nsIContentViewerContainer*& aContainerResult)
{
  return NS_OK;
}

nsresult nsCalendarWidget::LoadURL(const nsString& aURLSpec, nsIStreamObserver* aListener, nsIPostData * aPostData)
{

  nsresult res ;

  nsIStreamManager * stream_manager  = ((nsCalendarShell *)mCalendarShell)->mShellInstance->GetStreamManager();

  if (stream_manager == nsnull)
    return NS_OK;

  static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);

  nsIStreamListener * stream_listener;

  NS_NewCalStreamListener(&stream_listener);

  ((nsCalStreamListener *)stream_listener)->SetWidget(this);

  nsIID * iid_dtd = nsnull;
  nsIID * iid_sink = nsnull ;

  if (aURLSpec.Find(".cal") != -1)
  {
    static NS_DEFINE_IID(kCCalXMLDTD, NS_ICALXML_DTD_IID);
    static NS_DEFINE_IID(kCCalXMLContentSinkCID, NS_CALXMLCONTENTSINK_IID); 

    iid_dtd  = (nsIID *) &kCCalXMLDTD;
    iid_sink = (nsIID *) &kCCalXMLContentSinkCID;
    mRootCanvas->DeleteChildren();
  }

  res = stream_manager->LoadURL(((nsCalendarShell*)mCalendarShell)->mDocumentContainer,
                              aURLSpec, 
                              stream_listener, 
                              aPostData,iid_dtd,iid_sink);

  return res;

}


NS_CALENDAR nsresult NS_NewCalendarWidget(nsICalendarWidget** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsCalendarWidget* it = new nsCalendarWidget();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kICalWidgetIID, (void **) aInstancePtrResult);
}

nsIWidget* nsCalendarWidget::GetWWWindow()
{
  if (nsnull != mWindow) {
     NS_ADDREF(mWindow);
  }
  return mWindow;
}

nsEventStatus nsCalendarWidget::HandleEvent(nsGUIEvent *aEvent)
{  
  return (mRootCanvas->HandleEvent(aEvent));
}

nsIStreamListener * nsCalendarWidget::GetParserStreamInterface()
{  
  NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);

  nsIStreamManager * stream_manager  = ((nsCalendarShell *)mCalendarShell)->mShellInstance->GetStreamManager();

  if (stream_manager == nsnull)
    return nsnull;

  nsIStreamListener * aStreamListener ;

  nsresult res = ((nsStreamManager *)stream_manager)->mParser->QueryInterface(kIStreamListenerIID, (void **)&aStreamListener);

  if (NS_OK != res)
    return (nsnull);

  return (aStreamListener);
}

nsresult nsCalendarWidget::GetRootCanvas(nsIXPFCCanvas ** aCanvas)
{  

  return (mRootCanvas->QueryInterface(kIXPFCCanvasIID, (void **)aCanvas));
}

