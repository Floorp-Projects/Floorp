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
  mUrl = nsnull;
  mParser = nsnull;
  mDTD = nsnull;
  mSink = nsnull;
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
  NS_IF_RELEASE(mUrl);
  NS_IF_RELEASE(mParser);
  NS_IF_RELEASE(mSink);
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

/*
 * If we can find the file, then use it, else use the HARDCODE.
 * We'll need to change the way we deal with this since the file
 * could be gotten over the network.
 */

  nsresult rv = NS_OK;

  PRBool bHardcode = PR_FALSE;

  char * pUI = aURLSpec.ToNewCString();

  /*
   * Create a nsIURL representing the interface ...
   */

  static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);

  nsIStreamListener * aStreamListener;
  nsresult res = NS_OK;
  nsCurlParser urlParser(pUI);
  
  if (urlParser.IsLocalFile() == PR_TRUE) {
    char * pURL = urlParser.LocalFileToURL();
    res = NS_NewURL(&mUrl, pURL);
  } else {
    res = NS_NewURL(&mUrl, pUI);
  }

  if (NS_OK != res) {
    bHardcode = PR_TRUE;
  }


  if (bHardcode == PR_FALSE)
  {
    if (urlParser.IsLocalFile() == PR_TRUE)
    {
      PRStatus status = PR_Access(pUI,PR_ACCESS_EXISTS);

      if (status == -1)
        bHardcode = PR_TRUE;

    } else {
      char * file = urlParser.ToLocalFile();

      PRStatus status = PR_Access(file,PR_ACCESS_EXISTS);

      if (status == -1)
        bHardcode = PR_TRUE;

      //delete file;
    }
  }

  if (PR_FALSE == bHardcode)
  {


    /*
     *  Create the Parser
     */
    static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
    static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

    rv = nsRepository::CreateInstance(kCParserCID, 
                                      nsnull, 
                                      kCParserIID, 
                                      (void **)&mParser);


    if (NS_OK != res) {
        return res;
    }

    /*
     * Create the DTD and Sink
     */

    // XXX: Really, the parser should figure out the mime type
    //      of our document and create the correct DTD. 

    if (PL_strstr(pUI, ".ui") != nsnull)
    {


      static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
      static NS_DEFINE_IID(kCCalXPFCXMLDTD, NS_IXPFCXML_DTD_IID);

      res = nsRepository::CreateInstance(kCCalXPFCXMLDTD, 
                                         nsnull, 
                                         kIDTDIID,
                                         (void**) &mDTD);

      if (NS_OK != res) {
          return res;
      }

      static NS_DEFINE_IID(kIContentSinkIID, NS_ICONTENT_SINK_IID);
      static NS_DEFINE_IID(kCCalXPFCXMLContentSinkCID, NS_XPFCXMLCONTENTSINK_IID); 

      res = nsRepository::CreateInstance(kCCalXPFCXMLContentSinkCID, 
                                         nsnull, 
                                         kIContentSinkIID,
                                         (void**) &mSink);

      if (NS_OK != res) {
          return res;
      }

      ((nsXPFCXMLContentSink *)mSink)->SetViewerContainer(((nsCalendarShell*)mCalendarShell)->mDocumentContainer);


    } else {

      static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
      static NS_DEFINE_IID(kCCalXMLDTD, NS_ICALXML_DTD_IID);

      res = nsRepository::CreateInstance(kCCalXMLDTD, 
                                         nsnull, 
                                         kIDTDIID,
                                         (void**) &mDTD);

      if (NS_OK != res) {
          return res;
      }

      static NS_DEFINE_IID(kIContentSinkIID, NS_ICONTENT_SINK_IID);
      static NS_DEFINE_IID(kCCalXMLContentSinkCID, NS_CALXMLCONTENTSINK_IID); 

      res = nsRepository::CreateInstance(kCCalXMLContentSinkCID, 
                                         nsnull, 
                                         kIContentSinkIID,
                                         (void**) &mSink);

      if (NS_OK != res) {
          return res;
      }

      ((nsCalXMLContentSink *)mSink)->SetWidget(this);

      mRootCanvas->DeleteChildren();

    }


    NS_NewCalStreamListener(&aStreamListener);


    /*
     * Register the DTD
     */

    mParser->RegisterDTD(mDTD);


    /*
     * Register the Context Sink, Parser, etc...
     */

    mParser->SetContentSink(mSink);


    mDTD->SetContentSink(mSink);
    mDTD->SetParser(mParser);

    /*
     * XXX: Tell the Stream Listener about us ... this should be done at the 
     *      Content Model Level
     */

    ((nsCalStreamListener *)aStreamListener)->SetWidget(this);

    /*
     * Open the URL
     */

    res = mUrl->Open(aStreamListener);


    /*
     * We want to parse when the Stream has data?
     */

    mParser->Parse(mUrl);

  #if 0
    NS_IF_RELEASE(dtd);
    NS_IF_RELEASE(sink);
    NS_RELEASE(parser);
  #endif

  } else {


    nsIXPFCCanvas * canvas;
    nsIXPFCCanvas * headercanvas;
    nsIXPFCCanvas * belowheadercanvas;
    nsIXPFCCanvas * monthtodoparentcanvas;
    nsIXPFCCanvas * timebarscalecanvas;
    nsresult res;

    static NS_DEFINE_IID(kCCalMultiDayViewCID,      NS_CAL_MULTIDAYVIEWCANVAS_CID);
    static NS_DEFINE_IID(kCCalTodoComponentCanvasCID,      NS_CAL_TODOCOMPONENTCANVAS_CID);
    static NS_DEFINE_IID(kCXPFCCanvasCID,            NS_XPFC_CANVAS_CID);
    static NS_DEFINE_IID(kCCalContextControllerCID, NS_CAL_CONTEXT_CONTROLLER_CID);
    static NS_DEFINE_IID(kCCalTimebarContextControllerCID, NS_CAL_TIMEBAR_CONTEXT_CONTROLLER_CID);
    static NS_DEFINE_IID(kCCalMonthContextControllerCID, NS_CAL_MONTH_CONTEXT_CONTROLLER_CID);
    static NS_DEFINE_IID(kCalTimebarHeadingCID,     NS_CAL_TIMEBARHEADING_CID);
    static NS_DEFINE_IID(kCalTimebarUserHeadingCID,     NS_CAL_TIMEBARUSERHEADING_CID);
    static NS_DEFINE_IID(kCalTimebarTimeHeadingCID,     NS_CAL_TIMEBARTIMEHEADING_CID);
    static NS_DEFINE_IID(kCalTimebarScaleCID,       NS_CAL_TIMEBARSCALE_CID);

    static NS_DEFINE_IID(kIXPFCCanvasIID,            NS_IXPFC_CANVAS_IID);
    static NS_DEFINE_IID(kCalContextControllerIID,  NS_ICAL_CONTEXT_CONTROLLER_IID);
    static NS_DEFINE_IID(kXPFCSubjectIID,            NS_IXPFC_SUBJECT_IID);

  /******************************************************************************

    Let's create a Context that will be shared across the TimebarScale and the 
    MultiDayView.

  *******************************************************************************/

    static NS_DEFINE_IID(kCCalTimeContextCID,  NS_CAL_TIME_CONTEXT_CID);
    static NS_DEFINE_IID(kCCalTimeContextIID,  NS_ICAL_TIME_CONTEXT_IID);
    static NS_DEFINE_IID(kXPFCObserverIID, NS_IXPFC_OBSERVER_IID);

    nsICalTimeContext * context;
    nsIXPFCObserver * observer;

    res = nsRepository::CreateInstance(kCCalTimeContextCID, 
                                       nsnull, 
                                       kCCalTimeContextIID, 
                                       (void **)&context);

    if (NS_OK != res)
      return res ;

    context->Init();

    res = context->QueryInterface(kXPFCObserverIID, (void **)&observer);

    nsIXPFCSubject * north_context_subject;
    context->QueryInterface(kXPFCSubjectIID, (void **)&north_context_subject);



  /*******************************************************************************
      First, create 2 canvas as children in the root canvas, with a y box layout.
      the top canvas will contain the user header and context controllers, with
      a preferred height.  The bottom will freefloat and contain everything else.
      It will have a minimum so that both start shrinking when the minimum is hit.
  *******************************************************************************/

    /*
     * create canvas to hold header
     */
    res = nsRepository::CreateInstance(kCXPFCCanvasCID, 
                                       nsnull, 
                                       kIXPFCCanvasIID, 
                                       (void **)&canvas);

    if (NS_OK != res)
      return res ;

    canvas->Init();

    mRootCanvas->AddChildCanvas(canvas);
    canvas->SetPreferredSize(nsSize(50,50));
    canvas->SetMaximumSize(nsSize(50,50));

    headercanvas = canvas;

    /*
     * create canvas to hold everything else
     */

    res = nsRepository::CreateInstance(kCXPFCCanvasCID, 
                                       nsnull, 
                                       kIXPFCCanvasIID, 
                                       (void **)&canvas);

    if (NS_OK != res)
      return res ;

    canvas->Init();

    mRootCanvas->AddChildCanvas(canvas);

    belowheadercanvas = canvas;

    // change layout to y box on root canvas
    ((nsBoxLayout *)(mRootCanvas->GetLayout()))->SetLayoutAlignment(eLayoutAlignment_vertical);


  /*******************************************************************************
    Add the User Header Bar with context controllers
  *******************************************************************************/

    /* 
      Add Context Controller Canvas - West
    */
    res = nsRepository::CreateInstance(kCCalTimebarContextControllerCID, 
                                       nsnull, 
                                       kIXPFCCanvasIID, 
                                       (void **)&canvas);

    if (NS_OK != res)
      return res ;

    headercanvas->AddChildCanvas(canvas);
    canvas->SetForegroundColor(FOREGROUND_COLOR);
    canvas->SetPreferredSize(nsSize(50,50));
    canvas->SetMinimumSize(nsSize(50,50));
    canvas->SetMaximumSize(nsSize(50,50));

    nsCalTimebarContextController * controller;

    res = canvas->QueryInterface(kCalContextControllerIID, (void **)&controller);
  
    nsIXPFCSubject * west_subject;

    if (NS_OK == res) {
      controller->Init();
      controller->SetOrientation(nsContextControllerOrientation_west);
      controller->SetPeriodFormat(nsCalPeriodFormat_kDay);
      controller->GetDuration()->SetDay(-1);
      res = controller->QueryInterface(kXPFCSubjectIID, (void **)&west_subject);
      NS_RELEASE(controller);
    }

    /* 
      Add Timebar Heading Canvas 
    */
    nsRepository::CreateInstance(kCalTimebarUserHeadingCID, 
                                 nsnull, 
                                 kIXPFCCanvasIID, 
                                 (void **)&canvas);

    if (NS_OK != res)
      return res ;

    headercanvas->AddChildCanvas(canvas);
    canvas->SetMinimumSize(nsSize(50,50));

    /* 
      Add Context Controller Canvas - East 
    */
    nsRepository::CreateInstance(kCCalTimebarContextControllerCID, 
                                 nsnull, 
                                 kIXPFCCanvasIID, 
                                 (void **)&canvas);

    if (NS_OK != res)
      return res ;

    headercanvas->AddChildCanvas(canvas);
    canvas->SetForegroundColor(FOREGROUND_COLOR);
    canvas->SetPreferredSize(nsSize(50,50));
    canvas->SetMinimumSize(nsSize(50,50));
    canvas->SetMaximumSize(nsSize(50,50));

    res = canvas->QueryInterface(kCalContextControllerIID, (void **)&controller);

    nsIXPFCSubject * east_subject;
  
    if (NS_OK == res) {
      controller->Init();
      controller->SetOrientation(nsContextControllerOrientation_east);
      controller->SetPeriodFormat(nsCalPeriodFormat_kDay);
      controller->GetDuration()->SetDay(1);
      res = controller->QueryInterface(kXPFCSubjectIID, (void **)&east_subject);
      NS_RELEASE(controller);
    }


  /*******************************************************************************
    Here we add the blank container for timescale, multi-day view, 
    blank container for monthview/todo As a child of the belowheadercanvas
  *******************************************************************************/

    /* 
      Add a canvas for Timebar Scale Canvas 
    */
    nsRepository::CreateInstance(kCXPFCCanvasCID, 
                                 nsnull, 
                                 kIXPFCCanvasIID, 
                                 (void **)&canvas);

    if (NS_OK != res)
      return res ;
    canvas->Init();      

    belowheadercanvas->AddChildCanvas(canvas);
    canvas->SetPreferredSize(nsSize(50,50));
    canvas->SetMinimumSize(nsSize(50,50));
    //canvas->SetMaximumSize(nsSize(50,50));

    timebarscalecanvas = canvas;

    // change layout to y box on this canvas
    ((nsBoxLayout *)(timebarscalecanvas->GetLayout()))->SetLayoutAlignment(eLayoutAlignment_vertical);

  /*******************************************************************************
    Add MultiDayView Canvas with a Command canvas if Debug mode is set
  *******************************************************************************/

    /*
     * Blank canvas for holding multiday and command/status container
     */

    nsRepository::CreateInstance(kCXPFCCanvasCID, 
                                 nsnull, 
                                 kIXPFCCanvasIID, 
                                 (void **)&canvas);

    if (NS_OK != res)
      return res ;
    canvas->Init();      

    belowheadercanvas->AddChildCanvas(canvas);
    canvas->SetMinimumSize(nsSize(50,50));
    // change layout to y box 
    ((nsBoxLayout *)(canvas->GetLayout()))->SetLayoutAlignment(eLayoutAlignment_vertical);

    nsIXPFCCanvas * multiday_holder_canvas = canvas;

    /*
     * The true multiday canvas
     */

    nsRepository::CreateInstance(kCCalMultiDayViewCID, 
                                 nsnull, 
                                 kIXPFCCanvasIID, 
                                 (void **)&canvas);

    if (NS_OK != res)
      return res ;
    ((nsCalMultiDayViewCanvas*)canvas)->SetShowStatus(PR_FALSE);
    canvas->Init();      

    multiday_holder_canvas->AddChildCanvas(canvas);
    canvas->SetMinimumSize(nsSize(50,50));
    ((nsCalTimebarCanvas *)canvas)->SetTimeContext(context);

    nsIXPFCObserver * observer_multiday;
    canvas->QueryInterface(kXPFCObserverIID, (void **)&observer_multiday);

    /*
     * A Command Canvas
     */
  #ifdef DEBUG
    static NS_DEFINE_IID(kCCalBottomCanvasCID, NS_CAL_COMMANDCANVAS_CID);
  #else
    static NS_DEFINE_IID(kCCalBottomCanvasCID, NS_CAL_STATUSCANVAS_CID);
  #endif

    nsRepository::CreateInstance(kCCalBottomCanvasCID, 
                                 nsnull, 
                                 kIXPFCCanvasIID, 
                                 (void **)&canvas);

    if (NS_OK != res)
      return res ;
    multiday_holder_canvas->AddChildCanvas(canvas);
    canvas->Init();      
    canvas->SetPreferredSize(nsSize(25,25));
    canvas->SetMinimumSize(nsSize(25,25));
    canvas->SetMaximumSize(nsSize(25,25));


    /* 
      Add canvas that is blank 
    */
    nsRepository::CreateInstance(kCXPFCCanvasCID, 
                                 nsnull, 
                                 kIXPFCCanvasIID, 
                                 (void **)&canvas);

    if (NS_OK != res)
      return res ;
    canvas->Init();

    belowheadercanvas->AddChildCanvas(canvas);
    canvas->SetPreferredSize(nsSize(HARDCODE_SPACE>>1,HARDCODE_SPACE>>1));
    canvas->SetMinimumSize(nsSize(HARDCODE_SPACE>>1,HARDCODE_SPACE>>1));
    canvas->SetMaximumSize(nsSize(HARDCODE_SPACE>>1,HARDCODE_SPACE>>1));

    // change layout to y box 
    ((nsBoxLayout *)(canvas->GetLayout()))->SetLayoutAlignment(eLayoutAlignment_vertical);

    monthtodoparentcanvas = canvas ;

  /*******************************************************************************
    Here we add the Timescale and associated controllers
  *******************************************************************************/

    /* 
      Add Context Controller Canvas - North 
    */
    res = nsRepository::CreateInstance(kCCalTimebarContextControllerCID, 
                                       nsnull, 
                                       kIXPFCCanvasIID, 
                                       (void **)&canvas);

    if (NS_OK != res)
      return res ;

    timebarscalecanvas->AddChildCanvas(canvas);
    canvas->SetForegroundColor(FOREGROUND_COLOR);
    canvas->SetPreferredSize(nsSize(25,25));

    res = canvas->QueryInterface(kCalContextControllerIID, (void **)&controller);
  
    nsIXPFCSubject * north_subject;

    if (NS_OK == res) {
      controller->Init();
      controller->SetOrientation(nsContextControllerOrientation_north);
      controller->SetPeriodFormat(nsCalPeriodFormat_kHour);
      controller->GetDuration()->SetHour(-1);
      res = controller->QueryInterface(kXPFCSubjectIID, (void **)&north_subject);
      NS_RELEASE(controller);
    }



    /* 
      Add Timebar Scale Canvas 
    */
    nsRepository::CreateInstance(kCalTimebarScaleCID, 
                                 nsnull, 
                                 kIXPFCCanvasIID, 
                                 (void **)&canvas);

    if (NS_OK != res)
      return res ;
    canvas->Init();      

    timebarscalecanvas->AddChildCanvas(canvas);
    ((nsCalTimebarCanvas *)canvas)->SetTimeContext(context);

    nsIXPFCObserver * observer_timescale;
    canvas->QueryInterface(kXPFCObserverIID, (void **)&observer_timescale);


    /* 
      Add Context Controller Canvas - South 
    */
    res = nsRepository::CreateInstance(kCCalTimebarContextControllerCID, 
                                       nsnull, 
                                       kIXPFCCanvasIID, 
                                       (void **)&canvas);

    if (NS_OK != res)
      return res ;

    timebarscalecanvas->AddChildCanvas(canvas);
    canvas->SetForegroundColor(FOREGROUND_COLOR);
    canvas->SetPreferredSize(nsSize(25,25));

    res = canvas->QueryInterface(kCalContextControllerIID, (void **)&controller);

    nsIXPFCSubject * south_subject;
  
    if (NS_OK == res) {
      controller->Init();
      controller->SetOrientation(nsContextControllerOrientation_south);
      controller->SetPeriodFormat(nsCalPeriodFormat_kHour);
      controller->GetDuration()->SetHour(1);
      res = controller->QueryInterface(kXPFCSubjectIID, (void **)&south_subject);
      NS_RELEASE(controller);
    }

    nsICalContextController * south_controller = controller;


  /*******************************************************************************
    Here we add the MiniCal and TodoView as a child of the above canvas
  *******************************************************************************/

    /* 
      Add MiniCal Canvas
    */
    nsRepository::CreateInstance(kCCalMonthContextControllerCID, 
                                 nsnull, 
                                 kIXPFCCanvasIID, 
                                 (void **)&canvas);

    if (NS_OK != res)
      return res ;

    canvas->Init();

    monthtodoparentcanvas->AddChildCanvas(canvas);
    canvas->SetPreferredSize(nsSize(HARDCODE_SPACE,HARDCODE_SPACE));
    canvas->SetMinimumSize(nsSize(HARDCODE_SPACE,HARDCODE_SPACE));
    canvas->SetMaximumSize(nsSize(HARDCODE_SPACE,HARDCODE_SPACE));

    res = canvas->QueryInterface(kCalContextControllerIID, (void **)&controller);

    nsIXPFCSubject * month_subject;
    nsIXPFCObserver * observer_month_controller;
  
    if (NS_OK == res) {
      controller->Init();
      controller->SetOrientation(nsContextControllerOrientation_north);

      res = controller->QueryInterface(kXPFCSubjectIID, (void **)&month_subject);
      res = controller->QueryInterface(kXPFCObserverIID, (void **)&observer_month_controller);

      NS_RELEASE(controller);
    }

    /* 
      Add Todo Canvas
    */
    nsRepository::CreateInstance(kCCalTodoComponentCanvasCID, 
                                 nsnull, 
                                 kIXPFCCanvasIID, 
                                 (void **)&canvas);

    if (NS_OK != res)
      return res ;


    monthtodoparentcanvas->AddChildCanvas(canvas);

    canvas->Init();


  /*******************************************************************************
      Setup Observers
  *******************************************************************************/

    // ContextController is SUBJECT, Context is Observer
    mCalendarShell->GetObserverManager()->Register(north_subject, observer);
    mCalendarShell->GetObserverManager()->Register(south_subject, observer);
    mCalendarShell->GetObserverManager()->Register(east_subject, observer);
    mCalendarShell->GetObserverManager()->Register(west_subject, observer);
    mCalendarShell->GetObserverManager()->Register(month_subject, observer);

    // Context is subject, Canvas is observer
    mCalendarShell->GetObserverManager()->Register(north_context_subject, observer_timescale);
    mCalendarShell->GetObserverManager()->Register(north_context_subject, observer_multiday);

    // Context is subject, controller is observer
    mCalendarShell->GetObserverManager()->Register(north_context_subject, observer_month_controller);



    NS_RELEASE(north_subject);
    NS_RELEASE(south_subject);
    NS_RELEASE(east_subject);
    NS_RELEASE(west_subject);

  /*******************************************************************************
      Layout the children
  *******************************************************************************/

    mRootCanvas->Layout();


  #if defined(DEBUG) && defined(XP_PC)
  //  mRootCanvas->DumpCanvas(stdout,0);
  #endif

  }
  
  delete pUI;

  return rv;
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

  nsIStreamListener * aStreamListener ;

  nsresult res = mParser->QueryInterface(kIStreamListenerIID, (void **)&aStreamListener);

  if (NS_OK != res)
    return (nsnull);

  return (aStreamListener);
}

nsresult nsCalendarWidget::GetRootCanvas(nsIXPFCCanvas ** aCanvas)
{  

  return (mRootCanvas->QueryInterface(kIXPFCCanvasIID, (void **)aCanvas));
}

