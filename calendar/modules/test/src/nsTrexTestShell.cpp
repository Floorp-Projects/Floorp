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

#include "jdefines.h"
#include "nsTrexTestShell.h"
#include "nsxpfcCIID.h"
#include "nsIAppShell.h"
#include "nsTrexTestShellCIID.h"
#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsRepository.h"
#include "nsTrexTestShellFactory.h"
#include "nsFont.h"
#include "nsITextWidget.h"
#include "nsITextAreaWidget.h"
#include "nsWidgetsCID.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nspr.h"

#define HEIGHT 30

static NS_DEFINE_IID(kITextWidgetIID, NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kCTextAreaCID, NS_TEXTAREA_CID);
static NS_DEFINE_IID(kCTextFieldCID, NS_TEXTFIELD_CID);
static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);

nsITrexTestShell * gShell = nsnull;

nsEventStatus PR_CALLBACK HandleEventTextField(nsGUIEvent *aEvent);

// All Applications must specify this *special* application CID
// to their own unique IID.
nsIID kIXPCOMApplicationShellCID = NS_TREXTEST_SHELL_CID ; 

#ifdef NS_UNIX
#include "Xm/Xm.h"
#include "Xm/MainW.h"
#include "Xm/Frame.h"
#include "Xm/XmStrDefs.h"
#include "Xm/DrawingA.h"

extern XtAppContext app_context;
extern Widget topLevel;
#endif


#include "nsCRT.h"
static void PR_CALLBACK TrexTestClientThread(void *arg);


#include "jsapi.h"
void PR_CALLBACK ZuluErrorReporter(JSContext *cx, const char *message, JSErrorReport *report);
PR_STATIC_CALLBACK(JSBool) Zulu(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

enum Zulu_slots {
  ZULU_ZULU = -1,
  ZULU_ALIVE = -2
};


PR_STATIC_CALLBACK(JSBool) GetZuluProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsTrexTestShell * a = (nsTrexTestShell*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case ZULU_ZULU:
      {
        *vp = PRIVATE_TO_JSVAL(a);
        break;
      }
      case ZULU_ALIVE:
      {
        *vp = BOOLEAN_TO_JSVAL(PR_TRUE);
        break;
      }

    }
  }

  return PR_TRUE;

}

PR_STATIC_CALLBACK(JSBool) SetZuluProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  return PR_TRUE;
}

PR_STATIC_CALLBACK(void) FinalizeZulu(JSContext *cx, JSObject *obj)
{
}

PR_STATIC_CALLBACK(JSBool) EnumerateZulu(JSContext *cx, JSObject *obj)
{
  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool) ResolveZulu(JSContext *cx, JSObject *obj, jsval id)
{
  return JS_TRUE;
}


PR_STATIC_CALLBACK(JSBool) ZuluCommand(JSContext *cx, 
                                       JSObject *obj, 
                                       uintN argc, 
                                       jsval *argv, 
                                       jsval *rval);


JSClass ZuluClass = 
{
  "Zulu", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetZuluProperty,
  SetZuluProperty,
  EnumerateZulu,
  ResolveZulu,
  JS_ConvertStub,
  FinalizeZulu
};

static JSPropertySpec ZuluProperties[] =
{
  {"zulu",  ZULU_ZULU,  JSPROP_ENUMERATE},
  {"alive", ZULU_ALIVE, JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


static JSFunctionSpec ZuluMethods[] = 
{
  {"zulucommand", ZuluCommand, 1},
  {0}
};


// All Application Must implement this function
nsresult NS_RegisterApplicationShellFactory()
{

  nsresult res = nsRepository::RegisterFactory(kIXPCOMApplicationShellCID,
                                               new nsTrexTestShellFactory(kIXPCOMApplicationShellCID),
                                               PR_FALSE) ;

  return res;
}


/*
 * nsTrexTestShell Definition
 */

nsTrexTestShell::nsTrexTestShell()
{
  NS_INIT_REFCNT();
  mShellInstance  = nsnull ;
  mInput = nsnull;
  mDisplay = nsnull;

  gShell = this;

  mExitMon = nsnull ;
  mExitCounter = nsnull; 
  mDatalen = 0;
  mNumThreads = 0;
  mClientMon = nsnull;
  nsCRT::memset(&mClientAddr, 0 , sizeof(mClientAddr));

  mCommand = "JUNK";

  mJSRuntime = nsnull;
  mJSContext = nsnull;
  mJSGlobal = nsnull;
  mJSZuluObject = nsnull;

  Zulu(nsnull,nsnull,0,nsnull,nsnull);
}

nsTrexTestShell::~nsTrexTestShell()
{
	if (mJSContext) JS_DestroyContext(mJSContext);
	if (mJSRuntime) JS_Finish(mJSRuntime);                      

	mJSContext = NULL;
	mJSRuntime = NULL;

  NS_IF_RELEASE(mInput);
  NS_IF_RELEASE(mDisplay);
}

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIAppShellIID, NS_IAPPSHELL_IID);


nsresult nsTrexTestShell::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsISupports*)(nsIApplicationShell *)(this);                                        
  }
  else if(aIID.Equals(kIXPCOMApplicationShellCID)) {  //do nsIContentSink base class...
    *aInstancePtr = (nsITrexTestShell*)(this);                                        
  }
  else if(aIID.Equals(kIAppShellIID)) {  //do nsIContentSink base class...
    *aInstancePtr = (nsIAppShell*)(this);                                        
  }
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
  return NS_OK;                                                        
}

NS_IMPL_ADDREF(nsTrexTestShell)
NS_IMPL_RELEASE(nsTrexTestShell)

nsresult nsTrexTestShell::Init()
{

  /*
   * Register class factrories needed for application
   */

  RegisterFactories() ;

  nsresult res = nsApplicationManager::GetShellInstance(this, &mShellInstance) ;

  if (NS_OK != res)
    return res ;

  nsRect aRect(100,100,800, 600) ;

  nsIAppShell * appshell ;

  res = QueryInterface(kIAppShellIID,(void**)&appshell);

  if (NS_OK != res)
    return res ;

  mShellInstance->CreateApplicationWindow(appshell,aRect);

  /*
   * create the 2 widgets
   */

    static NS_DEFINE_IID(kInsTextAreaWidgetIID, NS_ITEXTAREAWIDGET_IID);

  nsRepository::CreateInstance(kCTextAreaCID, 
                               nsnull, 
                               kInsTextAreaWidgetIID, 
                               (void **)&mDisplay);
  aRect.x = 0;
  aRect.y = 0;
  aRect.height-=HEIGHT;
  aRect.width-=10;

  nsIWidget * dw = nsnull;
  res = mDisplay->QueryInterface(kIWidgetIID, (void**)&dw);

  dw->Create(mShellInstance->GetApplicationWidget(), 
                   aRect, 
                   HandleEventTextField, 
                   NULL);

  PRUint32 length;
  mDisplay->SetText("TrexTest 0.1\r\n",length);
  mDisplay->InsertText("-------------------\r\n",0x7fffffff,0x7fffffff,length);
  dw->Show(PR_TRUE);
  PRBool prev;
  mDisplay->SetReadOnly(PR_TRUE,prev);


  aRect.y = aRect.height - HEIGHT;
  aRect.height = HEIGHT;

  nsRepository::CreateInstance(kCTextFieldCID, 
                               nsnull, 
                               kITextWidgetIID, 
                               (void **)&mInput);

  nsIWidget * iw = nsnull;
  res = mInput->QueryInterface(kIWidgetIID, (void**)&iw);

  iw->Create(mShellInstance->GetApplicationWidget(), 
                 aRect, 
                 HandleEventTextField, 
                 NULL);

  iw->Show(PR_TRUE);
  iw->SetFocus();


  nsIWidget * app = mShellInstance->GetApplicationWidget();

  nsRect rect;

  app->GetBounds(rect);

  rect.x = 0;
  rect.y = 0;

  dw->Resize(rect.x,rect.y,rect.width,rect.height-HEIGHT,PR_TRUE);
  iw->Resize(rect.x,rect.height-HEIGHT,rect.width,HEIGHT,PR_TRUE);

  NS_RELEASE(dw);
  NS_RELEASE(iw);

  mShellInstance->ShowApplicationWindow(PR_TRUE) ;


  mClientMon = PR_NewMonitor();
  PR_EnterMonitor(mClientMon);

  PRFileDesc * sockfd = nsnull;
  PRNetAddr netaddr;
  PRInt32 i = 0;

  sockfd = PR_NewTCPSocket();

  if (sockfd == nsnull)
    return NS_OK;

  nsCRT::memset(&netaddr, 0 , sizeof(netaddr));

  netaddr.inet.family = PR_AF_INET;
  netaddr.inet.port   = PR_htons(TCP_SERVER_PORT);
  netaddr.inet.ip     = PR_htonl(PR_INADDR_ANY);

  while (PR_Bind(sockfd, &netaddr) < 0) 
  {
    if (PR_GetError() == PR_ADDRESS_IN_USE_ERROR) 
    {
      netaddr.inet.port += 2;
      if (i++ < SERVER_MAX_BIND_COUNT)
        continue;
    }
    PR_Close(sockfd);
    return NS_OK;
  }

  if (PR_Listen(sockfd, 32) < 0) 
  {
    PR_Close(sockfd);
    return NS_OK;
  }

  if (PR_GetSockName(sockfd, &netaddr) < 0) 
  {
    PR_Close(sockfd);
    return NS_OK;
  }

  mClientAddr.inet.family = netaddr.inet.family;
  mClientAddr.inet.port   = netaddr.inet.port;
  mClientAddr.inet.ip     = netaddr.inet.ip;

  PR_Close(sockfd);

  // Kick off JS - Later on, we need to look at the DOM
	mJSRuntime = JS_Init((uint32) 0xffffffffL);

  if (nsnull != mJSRuntime) 
	  mJSContext = JS_NewContext(mJSRuntime, 8192);

  mJSGlobal = JS_NewObject(mJSContext, &ZuluClass, nsnull, nsnull);

  if (nsnull != mJSGlobal) 
  {
    JS_SetPrivate(mJSContext, mJSGlobal, this);

    JS_DefineProperties(mJSContext, mJSGlobal, ZuluProperties);
    JS_DefineFunctions(mJSContext, mJSGlobal,  ZuluMethods);

    //JS_AddNamedRoot(mJSContext, aSlot, "zulu_object");

    JS_InitStandardClasses(mJSContext, mJSGlobal);
    JS_SetGlobalObject(mJSContext, mJSGlobal);

    // Init our Zulu object here!
    //JS_DefineProperties(mJSContext, mJSGlobal, ZuluProperties);
    //JS_DefineFunctions(mJSContext, mJSGlobal, ZuluMethods);

    JS_SetErrorReporter(mJSContext, ZuluErrorReporter); 

  }

  return res ;
}


nsresult nsTrexTestShell::Create(int* argc, char ** argv)
{
  return NS_OK;
}
nsresult nsTrexTestShell::Exit()
{
  return NS_OK;
}

nsresult nsTrexTestShell::Run()
{
  mShellInstance->Run();
  return NS_OK;
}

nsresult nsTrexTestShell::SetDispatchListener(nsDispatchListener* aDispatchListener)
{
  return NS_OK;
}

void* nsTrexTestShell::GetNativeData(PRUint32 aDataType)
{
#ifdef XP_UNIX
  if (aDataType == NS_NATIVE_SHELL)
    return topLevel;

  return nsnull;
#else
  return (mShellInstance->GetApplicationWidget());
#endif

}

nsresult nsTrexTestShell::RegisterFactories()
{
#ifdef NS_WIN32
  #define XPFC_DLL  "xpfc10.dll"
#else
  #define XPFC_DLL "libxpfc10.so"
#endif

  // register graphics classes
  static NS_DEFINE_IID(kCXPFCCanvasCID, NS_XPFC_CANVAS_CID);

  nsRepository::RegisterFactory(kCXPFCCanvasCID, XPFC_DLL, PR_FALSE, PR_FALSE);

  static NS_DEFINE_IID(kCVectorCID, NS_VECTOR_CID);
  static NS_DEFINE_IID(kCVectorIteratorCID, NS_VECTOR_ITERATOR_CID);

  nsRepository::RegisterFactory(kCVectorCID, XPFC_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCVectorIteratorCID, XPFC_DLL, PR_FALSE, PR_FALSE);

  static NS_DEFINE_IID(kCXPFCObserverManagerCID,   NS_XPFC_OBSERVERMANAGER_CID);
  nsRepository::RegisterFactory(kCXPFCObserverManagerCID, XPFC_DLL, PR_FALSE, PR_FALSE);

  return NS_OK;
}

nsresult nsTrexTestShell::GetWebViewerContainer(nsIWebViewerContainer ** aWebViewerContainer)
{
  return NS_OK;
}

nsresult nsTrexTestShell::SendJS(nsString& aCommand)
{
  nsIWidget * iw = nsnull;
  nsresult res = mInput->QueryInterface(kIWidgetIID, (void**)&iw);
  nsIWidget * dw = nsnull;
  res = mDisplay->QueryInterface(kIWidgetIID, (void**)&dw);

  mInput->RemoveText();
  iw->Invalidate(PR_TRUE);
  nsString string("COMMAND: ");
  PRUint32 length;
  mDisplay->InsertText(string,0x7fffffff,0x7fffffff,length);
  mCommand.Truncate(0);
  aCommand.Copy(mCommand);

  mClientAddr.inet.ip = PR_htonl(PR_INADDR_LOOPBACK);

  jsval rval;

  JS_EvaluateUCScriptForPrincipals(mJSContext, 
                                   JS_GetGlobalObject(mJSContext),
                                   nsnull,
                                   (jschar*)mCommand.GetUnicode(), 
                                   mCommand.Length(),
                                   "", 
                                   0,
                                   &rval);

  while (mNumThreads) {
      PR_Wait(mClientMon, PR_INTERVAL_NO_TIMEOUT);
  }

  PR_ExitMonitor(mClientMon);

  NS_RELEASE(iw);
  NS_RELEASE(dw);

  ReceiveCommand(mCommand,mCommand);


  return NS_OK;
}

nsresult nsTrexTestShell::ReceiveCommand(nsString& aCommand, nsString& aReply)
{
  PRUint32 length;
  mDisplay->InsertText(aReply,0x7fffffff,0x7fffffff,length);
  mDisplay->InsertText("\r\n",0x7fffffff,0x7fffffff,length);

  nsIWidget * dw = nsnull;

  mDisplay->QueryInterface(kIWidgetIID,(void**)&dw);

  dw->Invalidate(PR_TRUE);

  return NS_OK;
}


nsEventStatus nsTrexTestShell::HandleEvent(nsGUIEvent *aEvent)
{

    nsIWidget * inputwidget = nsnull;

    if (mInput)
      mInput->QueryInterface(kIWidgetIID,(void**)&inputwidget);

    /*
     * the Input Widget
     */

    if (aEvent->widget == inputwidget)
    {
       switch (aEvent->message) 
        {
          case NS_KEY_UP:
  
            if (NS_VK_RETURN == ((nsKeyEvent*)aEvent)->keyCode) 
            {
              nsString text;
              PRUint32 length;
              mInput->GetText(text, 1000, length);

              SendJS(text);

            }
            break;
        }

      NS_IF_RELEASE(inputwidget);
      return nsEventStatus_eIgnore; 
  
    }

    /*
     * the App and Display Widgets
     */
   NS_IF_RELEASE(inputwidget);


    nsEventStatus result = nsEventStatus_eConsumeNoDefault;

    switch(aEvent->message) {

        case NS_CREATE:
        {
          return nsEventStatus_eConsumeNoDefault;
        }
        break ;

        case NS_SIZE:
        {
          nsRect * rect = ((nsSizeEvent*)aEvent)->windowSize;

          rect->x = 0;
          rect->y = 0;

          nsIWidget * iw = nsnull;
          mInput->QueryInterface(kIWidgetIID, (void**)&iw);
          nsIWidget * dw = nsnull;
          mDisplay->QueryInterface(kIWidgetIID, (void**)&dw);

          dw->Resize(rect->x,rect->y,rect->width,rect->height-HEIGHT,PR_TRUE);
          iw->Resize(rect->x,rect->height-HEIGHT,rect->width,HEIGHT,PR_TRUE);

          NS_RELEASE(iw);
          NS_RELEASE(dw);

          return nsEventStatus_eConsumeNoDefault;
        }
        break ;

        case NS_DESTROY:
        {
          mShellInstance->ExitApplication() ;
          return nsEventStatus_eConsumeNoDefault;
        }
        break ;
    }

    return nsEventStatus_eIgnore; 
}

nsEventStatus PR_CALLBACK HandleEventTextField(nsGUIEvent *aEvent)
{
  return (gShell->HandleEvent(aEvent));
}



static void PR_CALLBACK TrexTestClientThread(void *arg)
{
  nsTrexTestShell * app = (nsTrexTestShell *) arg;

  app->RunThread();

  app->ExitThread();

}


nsresult nsTrexTestShell :: RunThread()
{
  PRFileDesc *sockfd;
  buffer *in_buf;
  union PRNetAddr netaddr;
  PRInt32 bytes, bytes2, i, j;

  bytes = TCP_MESG_SIZE;

  in_buf = PR_NEW(buffer);

  if (!in_buf)
    return NS_OK;


  netaddr.inet.family = mClientAddr.inet.family;
  netaddr.inet.port = mClientAddr.inet.port;
  netaddr.inet.ip = mClientAddr.inet.ip;

  sockfd = PR_NewTCPSocket();

  if (sockfd == nsnull)
    return NS_OK;

  if (PR_Connect(sockfd, &netaddr,PR_INTERVAL_NO_TIMEOUT) < 0)
    return NS_OK;

  char * string = mCommand.ToNewCString();

  bytes2 = PR_Send(sockfd, string, bytes, 0, PR_INTERVAL_NO_TIMEOUT);

  if (bytes2 <= 0)
    return NS_OK;

  bytes2 = PR_Recv(sockfd, in_buf->data, bytes, 0, PR_INTERVAL_NO_TIMEOUT);

  if (bytes2 <= 0)
    return NS_OK;

  delete string;

  mCommand.SetString(in_buf->data, bytes2);

  PR_Shutdown(sockfd, PR_SHUTDOWN_BOTH);
  PR_Close(sockfd);

  PR_DELETE(in_buf);

  return NS_OK;
}


nsresult nsTrexTestShell :: ExitThread()
{
  PR_EnterMonitor(mExitMon);
  --(*mExitCounter);
  PR_Notify(mExitMon);
  PR_ExitMonitor(mExitMon);
  return NS_OK;
}


nsresult nsTrexTestShell::StartCommandServer()
{
  return NS_OK;
}


PR_STATIC_CALLBACK(JSBool) Zulu(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}




void PR_CALLBACK ZuluErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
  return ;
}


PR_STATIC_CALLBACK(JSBool) ZuluCommand(JSContext *cx, 
                                       JSObject *obj, 
                                       uintN argc, 
                                       jsval *argv, 
                                       jsval *rval)
{
  /*
   * Start the client thread here
   */

  nsTrexTestShell * a = (nsTrexTestShell*) JS_GetPrivate(cx, obj);

  *rval = JSVAL_NULL;

  /*
   * XXX: Extract the command & args from JS
   */

  nsString command = "";
  
  if (argc >= 1) 
  {

    PRUint32 count = 0;

    while (count != argc)
    {
      JSString * jsstring0 = JS_ValueToString(cx, argv[count]);
      if (nsnull != jsstring0) 
      {

        if (count != 0)
          command += " ";        

        command += JS_GetStringChars(jsstring0);
      }

      *rval = JSVAL_VOID;

      count++;
    }
  }
  else 
  {
    JS_ReportError(cx, "Function zulucommand requires 1 parameter");
    return JS_FALSE;
  }

  a->SendCommand(command);


  return JS_TRUE;
}

nsresult nsTrexTestShell::SendCommand(nsString& aCommand)
{
  /*
   * Launch a thread to deal with this
   */

  PRThread *t;

  mExitMon = mClientMon;

  mExitCounter = &(mNumThreads);

  mCommand = aCommand;

  t = PR_CreateThread(PR_USER_THREAD,
                      TrexTestClientThread, 
                      (void *) this,
                      PR_PRIORITY_NORMAL,
                      PR_LOCAL_THREAD,
                      PR_UNJOINABLE_THREAD,
                      0);

  mNumThreads++;


  return NS_OK;
}




