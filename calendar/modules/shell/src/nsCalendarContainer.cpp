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
#include "nsICalendarUser.h"
#include "nsICalendarModel.h"
#include "nsCoreCIID.h"
#include "nsIUser.h"
#include "nsCurlParser.h"
#include "nsX400Parser.h"
#include "nscal.h"
#include "nsCalMultiDayViewCanvas.h"
#include "nsCalMultiUserViewCanvas.h"
#include "nsLayer.h"
#include "nsCalTimebarCanvas.h"
#include "nsCalUICIID.h"
#include "nsCalNewModelCommand.h"
#include "nsIXPFCCommand.h"

// XXX: This code should use XML for defining the Root UI. We need to
//      implement the stream manager first to do this, then lots of
//      this specific code should be removed (it would load trex.ui)

static NS_DEFINE_IID(kICalContainerIID,       NS_ICALENDAR_CONTAINER_IID);

static NS_DEFINE_IID(kFileWidgetCID,          NS_FILEWIDGET_CID);
static NS_DEFINE_IID(kIFileWidgetIID,         NS_IFILEWIDGET_IID);

static NS_DEFINE_IID(kViewManagerCID,         NS_VIEW_MANAGER_CID);
static NS_DEFINE_IID(kIViewManagerIID,        NS_IVIEWMANAGER_IID);
static NS_DEFINE_IID(kIViewIID,               NS_IVIEW_IID);
static NS_DEFINE_IID(kViewCID,                NS_VIEW_CID);
static NS_DEFINE_IID(kWidgetCID,              NS_CHILD_CID);


static NS_DEFINE_IID(kIModelIID,         NS_IMODEL_IID);
static NS_DEFINE_IID(kICalendarModelIID, NS_ICALENDAR_MODEL_IID);
static NS_DEFINE_IID(kCCalendarModelCID, NS_CALENDAR_MODEL_CID);


nsCalendarContainer::nsCalendarContainer()
{
  NS_INIT_REFCNT();
  mMenuManager = nsnull;
  mCalendarWidget = nsnull;
  mCalendarShell = nsnull;
  mRootCanvas = nsnull;
  mToolkit = nsnull;
  mToolbarManager = nsnull;
}

NS_IMPL_QUERY_INTERFACE(nsCalendarContainer, kICalContainerIID)
NS_IMPL_ADDREF(nsCalendarContainer)
NS_IMPL_RELEASE(nsCalendarContainer)

nsCalendarContainer::~nsCalendarContainer()
{
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

  nsIViewManager * viewManager;

  res = nsRepository::CreateInstance(kViewManagerCID, 
                                     nsnull, 
                                     kIViewManagerIID, 
                                     (void **)&viewManager);


  if (NS_OK != res)
    return res ;

  viewManager->Init(aParent->GetDeviceContext());

  viewManager->SetFrameRate(25);

  gXPFCToolkit->GetCanvasManager()->SetViewManager(viewManager);

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

    mRootCanvas->Init();

    gXPFCToolkit->GetCanvasManager()->SetRootCanvas(mRootCanvas);

    /*
     * Associate the Logged In User's content model with the canvas
     */

    nsICalendarModel * calmodel = nsnull;
    nsIModel * model = nsnull;

    res = nsRepository::CreateInstance(kCCalendarModelCID, 
                                       nsnull, 
                                       kICalendarModelIID, 
                                       (void **)&calmodel);

    if (NS_OK != res)
        return res;

    calmodel->Init();

    calmodel->SetCalendarUser(((nsCalendarShell *)aCalendarShell)->mpLoggedInUser);

    calmodel->QueryInterface(kIModelIID, (void**)&model);

    if (NS_OK != res)
      return res;

    mRootCanvas->SetModel(model);

    NS_RELEASE(model);

    nsNativeWidget native = aParent->GetNativeData(NS_NATIVE_WIDGET);

    mRootCanvas->Init(native, 
                      aBounds);    

    gXPFCToolkit->GetViewManager()->SetRootView(mRootCanvas->GetView());

    //mRootCanvas->SetVisibility(PR_FALSE);
    ((nsBoxLayout *)(mRootCanvas->GetLayout()))->SetLayoutAlignment(eLayoutAlignment_vertical);


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

  res = mCalendarWidget->Init(mRootCanvas->GetView(), aBounds, aCalendarShell);

  mRootCanvas->Layout();  

  return (res);
}


nsresult nsCalendarContainer::SetMenuBar(nsIXPFCMenuBar * aMenuBar)
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

nsresult nsCalendarContainer::SetToolbarManager(nsIXPFCToolbarManager * aToolbarManager)
{  
  mToolbarManager = aToolbarManager;

  NS_ADDREF(mToolbarManager);

  return NS_OK;

}

nsIXPFCToolbarManager * nsCalendarContainer::GetToolbarManager()
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

    nsIView * parent = canvas->GetView();

    res = canvas->LoadView(kViewCID);

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
    static NS_DEFINE_IID(kCWidgetCID, NS_WINDOW_CID);

    nsIView * parent = canvas->GetView();

    res = canvas->LoadView(kViewCID, &kCWidgetCID, nsnull);

    nsIWidget * widget ;
    
    canvas->GetView()->GetWidget(widget);


    widget->SetBorderStyle(eBorderStyle_dialog);

    NS_RELEASE(widget);

    nsRect rect(400,0,400,600);
    canvas->SetBounds(rect);
  
    NS_RELEASE(canvas);
  }  
  return NS_OK;
}

nsresult nsCalendarContainer::UpdateMenuBar()
{  
  static NS_DEFINE_IID(kCIXPFCMenuContainerIID, NS_IXPFCMENUCONTAINER_IID);

  nsIXPFCMenuContainer * container = nsnull;

  nsresult res = mMenuManager->GetMenuBar()->QueryInterface(kCIXPFCMenuContainerIID,(void**)&container);

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


nsresult nsCalendarContainer::LoadURL(const nsString& aURLSpec, 
                                      nsIStreamObserver* aListener, 
                                      nsIXPFCCanvas * aParentCanvas,
                                      nsIPostData * aPostData)
{
  /*
   * this is temp hack code til get capi url's in the stream manager....sorry!
   */

  if (aURLSpec.Find(".ics") == -1)  
  {
  
    /*
     * This is a normal url, go thru the stream manager
     */

    return(mCalendarWidget->LoadURL(aURLSpec, aListener, aParentCanvas, aPostData));
  }

  /*
   * This is a capi url.  Today, we assume that this is an additional
   * item for a group agenda
   */

  nsCalendarShell * shell = (nsCalendarShell *) mCalendarShell;

  /*
   * Create a New User
   */

  static NS_DEFINE_IID(kICalendarUserIID,     NS_ICALENDAR_USER_IID); 
  static NS_DEFINE_IID(kCCalendarUserCID,     NS_CALENDAR_USER_CID);
  static NS_DEFINE_IID(kIUserIID,             NS_IUSER_IID);

  nsresult res;
  CAPIStatus s;

  nsICalendarUser * caluser ;
  nsIUser* pUser;

  res = nsRepository::CreateInstance(kCCalendarUserCID,
                                     nsnull,           
                                     kICalendarUserIID,
                                     (void**)&caluser);

  if (NS_OK != res) 
    return res;

  caluser->Init();

  if (NS_OK != (res = caluser->QueryInterface(kIUserIID,(void**)&pUser)))
    return res ;
  

  nsCurlParser theURL;

  char * cstring = aURLSpec.ToNewCString();

  theURL.SetCurl(cstring);

  delete cstring;

  JulianString sUserName(256);
  nsString     nsUserName;

  sUserName = theURL.GetUser();

  cstring = sUserName.GetBuffer();
  nsUserName = cstring;

  pUser->SetUserName(nsUserName);

  sUserName.DoneWithBuffer();

  pUser->GetUserName(nsUserName);
  JulianString sHandle(256);
  nsUserName.ToCString(sHandle.GetBuffer(),256);
  sHandle.DoneWithBuffer();

  /*
   *  Ask the session manager for a session...
   */
  res = shell->mSessionMgr.GetSession(theURL.GetCurl().GetBuffer(), 
                                      0L, 
                                      shell->GetCAPIPassword(), 
                                      shell->mCAPISession);
  
  if (nsCurlParser::eCAPI == theURL.GetProtocol())
  {
    nsX400Parser x(theURL.GetExtra());
    x.Delete("ND");
    x.GetValue(sHandle);
    sHandle.Prepend(":");
  }

  nsCalSession* pCalSession;
  /*
   * Get the session from the session manager...
   */
  s = shell->mSessionMgr.GetSession(theURL.GetCurl(),0,"",pCalSession);
  if (CAPI_ERR_OK != s)
    return NS_OK;
  /*
   * Get a handle to the specific calendar store from the session
   */


  JulianString jfile = theURL.GetCSID();

  s = pCalSession->mCapi->CAPI_GetHandle(shell->mCAPISession,
                              jfile.GetBuffer(),//sHandle.GetBuffer(),
                              0,
                              &(shell->mCAPIHandle));


  /*
   * Create the Model interfaces to the view system  ....
   */

  nsILayer* pLayer;

  static NS_DEFINE_IID(kILayerIID,            NS_ILAYER_IID);
  static NS_DEFINE_IID(kCLayerCID,            NS_LAYER_CID);

  if (NS_OK != (res = nsRepository::CreateInstance(
          kCLayerCID,         // class id that we want to create
          nsnull,             // not aggregating anything  (this is the aggregatable interface)
          kILayerIID,         // interface id of the object we want to get back
          (void**)&pLayer)))
    return 1;  // XXX fix this
  pLayer->Init();
  pLayer->SetCurl(theURL.GetCurl());
  caluser->SetLayer(pLayer);

  /*
   * Begin a calendar for the logged in user...
   */

  NSCalendar * pCalendar;

  pLayer->GetCal(pCalendar);
  NS_RELEASE(pLayer);
  
  switch(theURL.GetProtocol())
  {
    case nsCurlParser::eFILE:
    case nsCurlParser::eCAPI:
    {
      DateTime d;
      DateTime d1;
      JulianPtrArray EventList;
      nsILayer *pLayer;
      d.prevDay(14);
      d1.nextDay(14);      
      caluser->GetLayer(pLayer);
      NS_ASSERTION(0 != pLayer,"null pLayer");
      pLayer->SetShell(shell);
      pLayer->FetchEventsByRange(&d,&d1,&EventList);
      pCalendar->addEventList(&EventList);
      NS_RELEASE(pLayer);
    }
    break;

    default:
    {
      /* need to report that this is an unhandled curl type */
    }
    break;
  }  

  nsICalendarModel * calmodel = nsnull;
  nsIModel * model = nsnull;

  res = nsRepository::CreateInstance(kCCalendarModelCID, 
                                     nsnull, 
                                     kICalendarModelIID, 
                                     (void **)&calmodel);

  if (NS_OK != res)
      return res;

  calmodel->Init();

  calmodel->SetCalendarUser(caluser);

  res = calmodel->QueryInterface(kIModelIID, (void**)&model);

  if (NS_OK != res)
    return res;

  /*
   * find a multi canvas and add one here and then set it's model ... yeehaaa
   */

  /*
   * Send an NewModelCommand to all canvas's....
   *
   * This event basically specifies that a new model
   * needs to be viewed somewhere.  Currently, only
   * the multi canvas handles this
   */

  static NS_DEFINE_IID(kXPFCCommandIID, NS_IXPFC_COMMAND_IID);

  nsCalNewModelCommand * command = nsnull;

  res = nsRepository::CreateInstance(kCCalNewModelCommandCID, 
                                     nsnull, 
                                     kXPFCCommandIID,
                                     (void **)&command);
  if (NS_OK != res)
      return res;

  command->Init();

  command->mModel = model;

  mRootCanvas->BroadcastCommand(*command);

  NS_RELEASE(command);
  NS_RELEASE(model);


  return NS_OK;
  
}

nsICalendarWidget * nsCalendarContainer::GetDocumentWidget()
{
  return(mCalendarWidget);
}

nsEventStatus nsCalendarContainer::HandleEvent(nsGUIEvent *aEvent)
{    
  nsIXPFCCanvas * canvas = nsnull ;
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
