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
#include "nsIXPFCCanvasManager.h"
#include "nsIXPFCCommand.h"
#include "nsCalDurationCommand.h"
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
#include "nsCurlParser.h"
#include "nsCalToolkit.h"
#include "nsBoxLayout.h"
#include "nsStreamManager.h"

#include "nsColor.h"
#include "nsFont.h"

#include "nsCalDayViewCanvas.h"
#include "nsCalMonthViewCanvas.h"
#include "nsCalMultiDayViewCanvas.h"
#include "nsCalMultiUserViewCanvas.h"
#include "nsCalTodoComponentCanvas.h"
#include "nsCalTimebarHeading.h"
#include "nsCalTimebarUserHeading.h"
#include "nsCalTimebarTimeHeading.h"

#include "nsCalStatusCanvas.h"
#include "nsCalCommandCanvas.h"

#include "nsICalendarShell.h"
#include "nsCalendarShell.h"
#include "nsIView.h"

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

nsresult nsCalendarWidget::Init(nsIView * aParent,
                                const nsRect& aBounds,
                                nsICalendarShell * aCalendarShell)
{

  mCalendarShell = aCalendarShell;

  NS_ADDREF(((nsIApplicationShell *)mCalendarShell));

  return NS_OK;

}

nsRect nsCalendarWidget::GetBounds()
{
  nsRect zr(0, 0, 0, 0);

  nsIXPFCCanvas * root = nsnull ;

  gXPFCToolkit->GetRootCanvas(&root);

  if (nsnull != root) {
    root->GetBounds(zr);
    NS_RELEASE(root);
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

nsresult nsCalendarWidget::LoadURL(const nsString& aURLSpec, 
                                   nsIStreamObserver* aListener, 
                                   nsIXPFCCanvas * aParentCanvas,
                                   nsIPostData * aPostData)
{

  nsresult res ;

  nsString aURLToLoad = aURLSpec;

  nsIXPFCCanvas * target_canvas = aParentCanvas;

  nsIStreamManager * stream_manager  = ((nsCalendarShell *)mCalendarShell)->mShellInstance->GetStreamManager();

  if (stream_manager == nsnull)
    return NS_OK;

  nsIID * iid_dtd = nsnull;
  nsIID * iid_sink = nsnull ;

  /*
   * Let's see if there is a target on this URL and delete that tree.
   *
   * Note we need some way of maintaining the model hookup here .... I think
   */

  if (aURLSpec.Find("?") != -1)
  {
    /*
     * We have options, let's see if one of them in target=
     */

    PRInt32 offset = aURLSpec.Find("target=");

    if (offset != -1)
    {

      /*
       * Ok, find the container with the specified name,
       * destroy all its contents, and use it as the target
       * base parent for the new XML!
       */

      nsString target ;
      nsString temp = aURLSpec ;

      offset += 7;

      temp.Mid(target, offset, aURLSpec.Length() - offset);

      offset = target.Find("?");

      if (offset != -1)
      {
        target.Cut(offset, target.Length() - offset);
      }

      /*
       * find the target canvas
       */

      nsIXPFCCanvas * root = nsnull ;
  
      gXPFCToolkit->GetRootCanvas(&root);

      target_canvas = root->CanvasFromName(target);

      if (target_canvas)
      {
        target_canvas->DeleteChildren();
      }

      NS_RELEASE(root);

    }

  }

  /*
   * change the DTD and Sink if this is a Calendar XML
   */

  if (aURLSpec.Find(".cal") != -1)
  {
    static NS_DEFINE_IID(kCCalXMLDTD, NS_ICALXML_DTD_IID);
    static NS_DEFINE_IID(kCCalXMLContentSinkCID, NS_CALXMLCONTENTSINK_IID); 

    iid_dtd  = (nsIID *) &kCCalXMLDTD;
    iid_sink = (nsIID *) &kCCalXMLContentSinkCID;

  }

  /*
   * XXX: Get rid of this code!
   * 
   * If we do not understand this url, lets create a canvas that embeds
   * raptor.
   *
   * Ideally, we'd all be using netlib and some sort of mime registration,
   * but til then....
   */

  if (aURLSpec.Find(".cal") == -1 && aURLSpec.Find(".ui") == -1)
  {
    aURLToLoad = "resource://res/ui/julian_html_blank.cal?target=content" ;

    /*
     * Register ourselves as a StreamListener, and then load the actual
     * URL when the ui framework is done
     */
  }


  /*
   * Load the URL
   */

  res = stream_manager->LoadURL(((nsCalendarShell*)mCalendarShell)->mDocumentContainer,
                              target_canvas,
                              aURLToLoad, 
                              aPostData,
                              iid_dtd,
                              iid_sink);

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
  nsIXPFCCanvas * root = nsnull ;

  gXPFCToolkit->GetRootCanvas(&root);

  nsEventStatus st = root->HandleEvent(aEvent);

  NS_RELEASE(root);

  return (st);
}

