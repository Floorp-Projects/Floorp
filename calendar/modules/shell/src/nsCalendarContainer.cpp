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

#include <stdio.h>
#include "nsCalendarContainer.h"
#include "nsCalShellCIID.h"
#include "nsCalendarShell.h"
#include "nsCalendarWidget.h"
#include "nsxpfcCIID.h"
#include "nsIMenuManager.h"
#include "nsWidgetsCID.h"
#include "nsIFileWidget.h"
#include "nsCalShellCIID.h"
#include "nscalcids.h"
#include "nsBoxLayout.h"
#include "nsViewsCID.h"
#include "nsIViewManager.h"
#include "nsXPFCToolkit.h"
#include "nsXPFCActionCommand.h"

// XXX: This code should use XML for defining the Root UI. We need to
//      implement the stream manager first to do this, then lots of
//      this specific code should be removed (it would load trex.ui)

static NS_DEFINE_IID(kICalContainerIID,       NS_ICALENDAR_CONTAINER_IID);

static NS_DEFINE_IID(kFileWidgetCID,          NS_FILEWIDGET_CID);
static NS_DEFINE_IID(kIFileWidgetIID,         NS_IFILEWIDGET_IID);
static NS_DEFINE_IID(kCXPFCDTD,               NS_IXPFCXML_DTD_IID);
static NS_DEFINE_IID(kCXPFCContentSink,       NS_XPFCXMLCONTENTSINK_IID);
static NS_DEFINE_IID(kCXPFCCommandServerCID,  NS_XPFC_COMMAND_SERVER_CID);
static NS_DEFINE_IID(kCXPFCHTMLCanvasCID,     NS_XPFC_HTML_CANVAS_CID);
static NS_DEFINE_IID(kCXPFolderCanvasCID,     NS_XP_FOLDER_CANVAS_CID);

static NS_DEFINE_IID(kViewManagerCID,         NS_VIEW_MANAGER_CID);
static NS_DEFINE_IID(kIViewManagerIID,        NS_IVIEWMANAGER_IID);
static NS_DEFINE_IID(kIViewIID,               NS_IVIEW_IID);
static NS_DEFINE_IID(kViewCID,                NS_VIEW_CID);
static NS_DEFINE_IID(kWidgetCID,              NS_CHILD_CID);

static NS_DEFINE_IID(kCCalCanvasCID,          NS_CAL_CANVAS_CID);

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

nsCalendarContainer::nsCalendarContainer()
{
  NS_INIT_REFCNT();
  mMenuManager = nsnull;
  mCalendarWidget = nsnull;
  mCalendarShell = nsnull;
  mRootCanvas = nsnull;
  mToolkit = nsnull;
  mToolbarManager = nsnull;
  mViewManager = nsnull;
}

NS_IMPL_QUERY_INTERFACE(nsCalendarContainer, kICalContainerIID)
NS_IMPL_ADDREF(nsCalendarContainer)
NS_IMPL_RELEASE(nsCalendarContainer)

nsCalendarContainer::~nsCalendarContainer()
{
  NS_IF_RELEASE(mViewManager);
  NS_IF_RELEASE(mMenuManager);
  NS_IF_RELEASE(mCalendarWidget);
  NS_IF_RELEASE(mRootCanvas);
  NS_IF_RELEASE(mToolkit);
  NS_IF_RELEASE(mToolbarManager);
}

nsresult nsCalendarContainer::Init(nsIWidget * aParent,
                                const nsRect& aBounds,
                                nsICalendarShell * aCalendarShell)
{

  nsIWidget * widget_parent = nsnull;

  RegisterFactories();

  /*
   * Create the UI Toolkit.
   */
  static NS_DEFINE_IID(kICalToolkitIID, NS_ICAL_TOOLKIT_IID);

  nsresult res = NS_OK;
  res = nsRepository::CreateInstance(kCCalToolkitCID, 
                                     nsnull, 
                                     kICalToolkitIID, 
                                     (void **)&mToolkit);

  if (NS_OK != res)
    return res ;

  mToolkit->Init((nsIApplicationShell *)aCalendarShell);


  static NS_DEFINE_IID(kIMenuManagerIID, NS_IMENU_MANAGER_IID);
  static NS_DEFINE_IID(kCMenuManagerCID, NS_MENU_MANAGER_CID);

  res = nsRepository::CreateInstance(kCMenuManagerCID, 
                                     nsnull, 
                                     kIMenuManagerIID, 
                                     (void **)&mMenuManager);

  if (NS_OK != res)
    return res ;

  mMenuManager->Init();

  /*
   * Create the view manager
   */
#if 0
  res = nsRepository::CreateInstance(kViewManagerCID, 
                                     nsnull, 
                                     kIViewManagerIID, 
                                     (void **)&mViewManager);


  if (NS_OK != res)
    return res ;

  mViewManager->Init(aParent->GetDeviceContext());

  mViewManager->SetFrameRate(25);
#endif

  gXPFCToolkit->GetCanvasManager()->SetWebViewerContainer((nsIWebViewerContainer *) this);

  /*
   * Create the Root UI
   */

  if (mRootCanvas == nsnull)
  {
    /*
     * Create a basic canvas with ybox. When a toolbar gets added to
     * us, it should be inserted first in the child list, after other
     * toolbars.
     *
     * The calendar widget's rootCanvas is last in the child list
     */

    static NS_DEFINE_IID(kIXPFCCanvasIID, NS_IXPFC_CANVAS_IID);

    res = nsRepository::CreateInstance(kCXPFCCanvasCID, 
                                       nsnull, 
                                       kCXPFCCanvasCID, 
                                       (void **)&mRootCanvas);

    if (NS_OK == res)
    {

      mRootCanvas->Init();
    }

    mRootCanvas->Init(aParent, aBounds,(((nsCalendarShell *)aCalendarShell)->mShellInstance->GetShellEventCallback()));    

    widget_parent = mRootCanvas->GetWidget();

    mRootCanvas->SetVisibility(PR_FALSE);
    ((nsBoxLayout *)(mRootCanvas->GetLayout()))->SetLayoutAlignment(eLayoutAlignment_vertical);
    gXPFCToolkit->GetCanvasManager()->SetRootCanvas(mRootCanvas);


#if 0
    nsIView * view = nsnull ;

    res = nsRepository::CreateInstance(kViewCID, 
                                       nsnull, 
                                       kIViewIID, 
                                       (void **)&(view));

    if (res != NS_OK)
      return res;

    static NS_DEFINE_IID(kCWidgetCID, NS_CHILD_CID);    

    view->Init(mViewManager, 
               aBounds, 
               nsnull,
               nsnull,//&kCWidgetCID,
               nsnull,
               aParent->GetNativeData(NS_NATIVE_WIDGET));

    mViewManager->SetRootView(view);

    mViewManager->SetWindowDimensions(aBounds.width, aBounds.height);

    //view->GetWidget(widget_parent);

    gXPFCToolkit->GetCanvasManager()->Register(mRootCanvas,view);
#endif
  }


  /*
   * Embed the Calendar Widget
   */

  if (mCalendarWidget == nsnull)
  {
    res = NS_NewCalendarWidget(&mCalendarWidget);

    if (res != NS_OK)
      return res;

  }

  /*
   * ReParent.  Need a cleaner way to do this.  Maybe the 
   * Widget itself should implement the nsIXPFCCanvas interface?
   */

  res = mCalendarWidget->Init(widget_parent, aBounds, aCalendarShell);

  mRootCanvas->Layout();  

  return (res);
}


nsresult nsCalendarContainer::SetMenuBar(nsIMenuBar * aMenuBar)
{  
  mMenuManager->SetMenuBar(aMenuBar);

  aMenuBar->SetShellContainer(((nsCalendarShell *)mCalendarShell)->mShellInstance,
                              (nsIWebViewerContainer *)this);

  return NS_OK;

}


nsresult nsCalendarContainer::SetMenuManager(nsIMenuManager * aMenuManager)
{  
  mMenuManager = aMenuManager;

  NS_ADDREF(mMenuManager);

  return NS_OK;

}

nsIMenuManager * nsCalendarContainer::GetMenuManager()
{  
  return (mMenuManager);

}

nsresult nsCalendarContainer::SetToolbarManager(nsIToolbarManager * aToolbarManager)
{  
  mToolbarManager = aToolbarManager;

  NS_ADDREF(mToolbarManager);

  return NS_OK;

}

nsIToolbarManager * nsCalendarContainer::GetToolbarManager()
{  
  return (mToolbarManager);
}

nsresult nsCalendarContainer::AddToolbar(nsIXPFCToolbar * aToolbar)
{  
  nsIXPFCCanvas * canvas;

  nsresult res = aToolbar->QueryInterface(kIXPFCCanvasIID,(void**)&canvas);

  if (NS_OK == res)
  {
    mRootCanvas->AddChildCanvas(canvas,0);

  /*
   * Let's add a native widget here.  This code should be in XPFC
   */

    /*
     * We want to instantiate a window widget which the canvas 
     * aggregates.  
     */
    static NS_DEFINE_IID(kCWidgetCID, NS_CHILD_CID);

    nsIWidget * parent = canvas->GetWidget();

    res = canvas->LoadWidget(kCWidgetCID);

    /*
     * Now, get the aggregated widget interface, and show this puppy
     */

    static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);

    nsIWidget * widget = nsnull;

    res = aToolbar->QueryInterface(kIWidgetIID,(void**)&widget);

    if (NS_OK == res)
    {

      nsRect rect(400,0,400,600);

      nsSize size;

      nsWidgetInitData initData ;

      initData.clipChildren = PR_FALSE;

      canvas->GetClassPreferredSize(size);

      canvas->SetPreferredSize(size);

      widget->Create(parent, 
                     rect, 
                     (((nsCalendarShell *)mCalendarShell)->mShellInstance->GetShellEventCallback()),
                     nsnull, nsnull, nsnull, &initData);

      widget->Show(PR_TRUE);
      NS_RELEASE(widget);
    }
  
    NS_RELEASE(canvas);
  }  

  return NS_OK;
}

nsresult nsCalendarContainer::RemoveToolbar(nsIXPFCToolbar * aToolbar)
{  
  return NS_OK;
}

nsresult nsCalendarContainer::UpdateToolbars()
{  
  // XXX Ugly ugly!

  nsIWidget * w;
  nsRect bounds;

  w = ((nsCalendarShell *)mCalendarShell)->mShellInstance->GetApplicationWidget();

  if (w == nsnull)
    return NS_OK;

  mRootCanvas->Layout();
  w->Invalidate(PR_FALSE);

  return NS_OK;  
}

nsresult nsCalendarContainer::ShowDialog(nsIXPFCDialog * aDialog)
{  
  nsIXPFCCanvas * canvas;

  nsresult res = aDialog->QueryInterface(kIXPFCCanvasIID,(void**)&canvas);

  if (NS_OK == res)
  {
    //mRootCanvas->AddChildCanvas(canvas,0);

  /*
   * Let's add a native widget here.  This code should be in XPFC
   */

    /*
     * We want to instantiate a window widget which the canvas 
     * aggregates.  
     */
    static NS_DEFINE_IID(kCWidgetCID, NS_WINDOW_CID);

    nsIWidget * parent = mRootCanvas->GetWidget();

    res = canvas->LoadWidget(kCWidgetCID);

    /*
     * Now, get the aggregated widget interface, and show this puppy
     */

    static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);

    nsIWidget * widget = nsnull;

    res = aDialog->QueryInterface(kIWidgetIID,(void**)&widget);

    if (NS_OK == res)
    {

      nsRect rect(400,0,400,600);

      nsWidgetInitData initData ;

      initData.clipChildren = PR_FALSE;

      canvas->SetBounds(rect);

      widget->SetBorderStyle(eBorderStyle_dialog);

      widget->Create(parent, 
                     rect, 
                     (((nsCalendarShell *)mCalendarShell)->mShellInstance->GetShellEventCallback()),
                     nsnull, nsnull, nsnull, &initData);

      widget->Resize(400,601,PR_FALSE);
      widget->Show(PR_TRUE);
      NS_RELEASE(widget);
    }
  
    NS_RELEASE(canvas);
  }  

  return NS_OK;
}

nsresult nsCalendarContainer::UpdateMenuBar()
{  
  static NS_DEFINE_IID(kCIMenuContainerIID, NS_IMENUCONTAINER_IID);

  nsIMenuContainer * container = nsnull;

  nsresult res = mMenuManager->GetMenuBar()->QueryInterface(kCIMenuContainerIID,(void**)&container);

  if (res == NS_OK)
  {
    container->Update();
    NS_RELEASE(container);
  }
  
  // XXX Ugly ugly!
  // Windows seems to get confused when the menubar changes
  // physical dimensions.  We need a way to tell the native window
  // that it's dimentions have changed ... argh.

  nsIWidget * w;
  nsRect bounds;

  w = ((nsCalendarShell *)mCalendarShell)->mShellInstance->GetApplicationWidget();

  if (w == nsnull)
    return NS_OK;

  w->Resize(800,
            600+1,
            PR_TRUE);

  return NS_OK;
}

nsresult nsCalendarContainer::SetTitle(const nsString& aTitle)
{  
  return NS_OK;
}
nsresult nsCalendarContainer::GetTitle(nsString& aResult)
{  
  return NS_OK;
}

nsresult nsCalendarContainer::QueryCapability(const nsIID &aIID, void** aResult)
{
  return NS_OK;
}
nsresult nsCalendarContainer::Embed(nsIContentViewer* aDocViewer, 
                   const char* aCommand,
                   nsISupports* aExtraInfo)
{
  return NS_OK;
}

nsresult nsCalendarContainer::SetContentViewer(nsIContentViewer* aViewer)
{
  return NS_OK;
}

nsresult nsCalendarContainer::GetContentViewer(nsIContentViewer*& aResult)
{
  return NS_OK;
}


nsresult nsCalendarContainer::SetApplicationShell(nsIApplicationShell* aShell)
{
  mCalendarShell = (nsCalendarShell*) aShell;
  return NS_OK;
}

nsresult nsCalendarContainer::GetApplicationShell(nsIApplicationShell*& aResult)
{
  aResult = (nsIApplicationShell *) mCalendarShell;
  NS_ADDREF((nsIApplicationShell *)mCalendarShell);
  return NS_OK;
}


nsresult nsCalendarContainer::LoadURL(const nsString& aURLSpec, nsIStreamObserver* aListener, nsIPostData * aPostData)
{
  return(mCalendarWidget->LoadURL(aURLSpec, aListener, aPostData));
}

nsICalendarWidget * nsCalendarContainer::GetDocumentWidget()
{
  return(mCalendarWidget);
}

nsEventStatus nsCalendarContainer::HandleEvent(nsGUIEvent *aEvent)
{    
  nsIXPFCCanvas * canvas = nsnull ;
  nsresult res;
  nsEventStatus es ;

  /*
   * find the canvas this event is associated with!
   */

  canvas = gXPFCToolkit->GetCanvasManager()->CanvasFromWidget(aEvent->widget);

  if (canvas != nsnull)
  {
    es = canvas->HandleEvent(aEvent);
      
    //mViewManager->DispatchEvent(aEvent, es);

    return es;
  }

  if (aEvent->message == NS_SIZE)
    mRootCanvas->SetBounds(*(((nsSizeEvent*)aEvent)->windowSize));
    
  return (mRootCanvas->HandleEvent(aEvent));
}

NS_CALENDAR nsresult NS_NewCalendarContainer(nsICalendarContainer** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsCalendarContainer* it = new nsCalendarContainer();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kICalContainerIID, (void **) aInstancePtrResult);
}

nsresult nsCalendarContainer::RegisterFactories()
{
  // register classes
  nsRepository::RegisterFactory(kCCalTimeContextCID, CALUI_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCCalComponentCID, CALUI_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCCalContextControllerCID, CALUI_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCCalTimebarContextControllerCID, CALUI_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCCalCanvasCID, CALUI_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCCalMonthContextControllerCID, CALUI_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCCalToolkitCID, CALUI_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCVectorCID, XPFC_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCVectorIteratorCID, XPFC_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCDateTimeCID, UTIL_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCstackCID, XPFC_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCDurationCID, UTIL_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCCalDurationCommandCID, CALUI_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCCalDayListCommandCID, CALUI_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCCalStatusCanvasCID, CALUI_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCCalCommandCanvasCID, CALUI_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCCalTodoComponentCanvasCID, CALUI_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCCalDayViewCID, CALUI_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCCalMultiDayViewCID, CALUI_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCalTimebarHeadingCID, CALUI_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCalTimebarUserHeadingCID, CALUI_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCalTimebarTimeHeadingCID, CALUI_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCalTimebarScaleCID, CALUI_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCCalXMLDTD, PARSER_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCCalXMLContentSink, PARSER_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCXPFCDTD, XPFC_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCXPFCContentSink, XPFC_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCLayoutCID, XPFC_DLL, PR_TRUE, PR_TRUE);
  nsRepository::RegisterFactory(kCBoxLayoutCID, XPFC_DLL, PR_TRUE, PR_TRUE);
  nsRepository::RegisterFactory(kCXPFCObserverCID, XPFC_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCXPFCSubjectCID, XPFC_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCXPFCObserverManagerCID, XPFC_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCXPFCCommandCID, XPFC_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCXPFCCanvasCID, XPFC_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCXPFCHTMLCanvasCID, XPFC_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCXPFolderCanvasCID, XPFC_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCXPFCCanvasCID, XPFC_DLL, PR_TRUE, PR_TRUE);
  nsRepository::RegisterFactory(kCXPFCCanvasManagerCID, XPFC_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCXPFCMethodInvokerCommandCID, XPFC_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCXPFCActionCommandCID, XPFC_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCXPFCCommandServerCID, XPFC_DLL, PR_FALSE, PR_FALSE);

  return NS_OK;
}


nsEventStatus nsCalendarContainer::ProcessCommand(nsIXPFCCommand * aCommand)
{
  /*
   * Check to see this is an ActionCommand
   */

  nsresult res;

  nsXPFCActionCommand * action_command = nsnull;

  static NS_DEFINE_IID(kXPFCActionCommandCID, NS_XPFC_ACTION_COMMAND_CID);                 

  res = aCommand->QueryInterface(kXPFCActionCommandCID,(void**)&action_command);

  if (NS_OK != res)
    return nsEventStatus_eIgnore;
  

  /*
   * Yeah, this is an action command. Do something
   */

  ProcessActionCommand(action_command->mAction);

  NS_RELEASE(action_command);

  return (nsEventStatus_eIgnore); 
}


nsresult nsCalendarContainer::ProcessActionCommand(nsString& aAction)
{
  nsString command ;
  PRUint32 offset = aAction.Find("?");

  if (offset > 0)
    aAction.Left(command,offset);
  else
    command = aAction;

  if (command.EqualsIgnoreCase("LoadUrl")) 
  {
    nsString url ;

    aAction.Mid(url, offset+1, aAction.Length() - offset - 1);

    LoadURL(url, nsnull);
  }
  
  return NS_OK;
}
