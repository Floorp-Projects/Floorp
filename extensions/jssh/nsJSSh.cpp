/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla JavaScript Shell project.
 *
 * The Initial Developer of the Original Code is
 * Alex Fritze.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex@croczilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsJSSh.h"
#include "nsIJSRuntimeService.h"
#include "nsIServiceManager.h"
#include "nsIXPConnect.h"
#include "nsIProxyObjectManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsIIOService.h"
#include "nsNetCID.h"
#include "nsIChannel.h"
#include "nsThreadUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsXPCOMCIDInternal.h"
#include "nsMemory.h"
#include "nsAutoPtr.h"

#ifdef PR_LOGGING
PRLogModuleInfo *gJSShLog = PR_NewLogModule("jssh");
#endif

//**********************************************************************
// Javascript Environment

const char *gWelcome = "Welcome to the Mozilla JavaScript Shell!\n";
const char *gGoodbye = "Goodbye!\n";

// GetJSShGlobal: helper for native js functions to obtain the global
// JSSh object
PRBool GetJSShGlobal(JSContext *cx, JSObject *obj, nsJSSh** shell)
{
  JSAutoRequest ar(cx);

#ifdef DEBUG
//  printf("GetJSShGlobal(cx=%p, obj=%p)\n", cx, obj);
#endif
  JSObject *parent, *global = obj;
  if (!global) {
    NS_ERROR("need a non-null obj to find script global");
    return PR_FALSE;
  }

  // in most of our cases, 'global' is already the correct obj, since
  // we use bound methods. For cases where we call
  // GetJSShGlobal from an unbound method, we need to walk the
  // parent chain:  
  // XXX I think the comment above is obsolete. We only call
  // GetJSShGlobal from bound methods, in which case the
  // parent is the global obj. For non-bound methods, walking the
  // parent chain to obtain the global object will only work when the
  // method is rooted in the current script context, so it is not
  // reliable (???). ASSERTION below to test the assumption. Once proven,
  // remove loop.
  while ((parent = JS_GetParent(cx, global))) {
    NS_ERROR("Parent chain weirdness. Probably benign, but we should not reach this.");
#ifdef DEBUG
//    printf("  obj's parent = %p\n", parent);
#endif
    global = parent;
  }
  
//    JSClass* clazz = JS_GET_CLASS(cx, global);
//    if (!IS_WRAPPER_CLASS(clazz)) {
//      NS_ERROR("the script global's class is not of the right type");
//      return PR_FALSE;
//    }

  // XXX use GetWrappedNativeOfJSObject
  nsIXPConnectWrappedNative *wrapper = static_cast<nsIXPConnectWrappedNative*>(JS_GetPrivate(cx, global));
  nsCOMPtr<nsISupports> native;
  wrapper->GetNative(getter_AddRefs(native));
  nsCOMPtr<nsIJSSh> jssh = do_QueryInterface(native);
  NS_ASSERTION(jssh, "no jssh global");
  *shell = static_cast<nsJSSh*>((nsIJSSh*)(jssh.get()));
  return PR_TRUE;
}

static void
my_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
  // xxx getting the global obj from the cx. will that give us grief?
  JSObject* obj = JS_GetGlobalObject(cx);
  nsJSSh* shell;
  if (!GetJSShGlobal(cx, obj, &shell)) return;

  // XXX use JSErrorReport for better info
  
  PRUint32 bytesWritten;
  if (shell->mOutput) {
    if (shell->mEmitHeader) {
      char buf[80];
      sprintf(buf, "[%d]", strlen(message));
      shell->mOutput->Write(buf, strlen(buf), &bytesWritten);
    }
    shell->mOutput->Write(message, strlen(message), &bytesWritten);
  }
}

static JSBool
Print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsJSSh* shell;
  if (!GetJSShGlobal(cx, obj, &shell)) return JS_FALSE;

  JSAutoRequest ar(cx);

  PRUint32 bytesWritten;

#ifdef DEBUG
//     nsCOMPtr<nsIThread> thread;
//     nsIThread::GetCurrent(getter_AddRefs(thread));
//     printf("printing on thread %p, shell %p, output %p, cx=%p, obj=%p\n", thread.get(), shell, shell->mOutput, cx, obj);
#endif

    for (unsigned int i=0; i<argc; ++i) {
     JSString *str = JS_ValueToString(cx, argv[i]);
     if (!str) return JS_FALSE;
     if (shell->mOutput) {
       if (shell->mEmitHeader) {
         char buf[80];
         sprintf(buf, "[%d]", JS_GetStringLength(str));
         shell->mOutput->Write(buf, strlen(buf), &bytesWritten);
       }
       shell->mOutput->Write(JS_GetStringBytes(str), JS_GetStringLength(str), &bytesWritten);
     }
     else
       printf("%s", JS_GetStringBytes(str)); // use cout if no output stream given.
#ifdef DEBUG
//        printf("%s", JS_GetStringBytes(str));
#endif
   }
  return JS_TRUE;
}

static JSBool
Quit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsJSSh* shell;
  if (!GetJSShGlobal(cx, obj, &shell)) return JS_FALSE;

  PRUint32 bytesWritten;

  if (shell->mOutput)
    shell->mOutput->Write(gGoodbye, strlen(gGoodbye), &bytesWritten);
  shell->mQuit = PR_TRUE;
  
  return JS_TRUE;
}

static JSBool
Load(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsJSSh* shell;

  JSAutoRequest ar(cx);

  if (!GetJSShGlobal(cx, obj, &shell)) return JS_FALSE;

  for (unsigned int i=0; i<argc; ++i) {
    JSString *str = JS_ValueToString(cx, argv[i]);
    if (!str) return JS_FALSE;
    //argv[i] = STRING_TO_JSVAL(str);
    const char *url = JS_GetStringBytes(str);
    if (!shell->LoadURL(url, rval))
      return JS_FALSE;
  }
  return JS_TRUE;
}

static JSBool
FlushEventQueue(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsJSSh* shell;
  if (!GetJSShGlobal(cx, obj, &shell)) return JS_FALSE;

  NS_ProcessPendingEvents(nsnull);
           
  return JS_TRUE;
}

static JSBool
Suspend(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsJSSh* shell;
  if (!GetJSShGlobal(cx, obj, &shell)) return JS_FALSE;

  nsCOMPtr<nsIThread> thread = do_GetCurrentThread();

  PR_AtomicIncrement(&shell->mSuspendCount);
  
  while (shell->mSuspendCount) {
    LOG(("|"));
    NS_ProcessNextEvent(thread);
  }
           
  return JS_TRUE;
}

static JSBool
Resume(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsJSSh* shell;
  if (!GetJSShGlobal(cx, obj, &shell)) return JS_FALSE;

  PR_AtomicDecrement(&shell->mSuspendCount);
  
  return JS_TRUE;
}

static JSBool
AddressOf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  if (argc!=1) return JS_FALSE;

  JSAutoRequest ar(cx);

  // xxx If argv[0] is not an obj already, we'll get a transient
  // address from JS_ValueToObject. Maybe we should throw an exception
  // instead.
  
  JSObject *arg_obj;
  if (!JS_ValueToObject(cx, argv[0], &arg_obj)) {
    return JS_FALSE;
  }
  
  char buf[80];
  sprintf(buf, "%p", arg_obj);
  JSString *str = JS_NewStringCopyZ(cx, buf);
  *rval = STRING_TO_JSVAL(str);
  return JS_TRUE;
}

static JSBool
SetProtocol(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  if (argc!=1) return JS_FALSE;
  nsJSSh* shell;
  if (!GetJSShGlobal(cx, obj, &shell)) return JS_FALSE;

  JSAutoRequest ar(cx);

  JSString *str = JS_ValueToString(cx, argv[0]);
  if (!str) return JS_FALSE;
  char *protocol = JS_GetStringBytes(str);
  
  if (!strcmp(protocol, "interactive")) {
    shell->mEmitHeader = PR_FALSE;
    shell->mPrompt = NS_LITERAL_CSTRING("\n> ");
    shell->mProtocol = protocol;
  }
  else if (!strcmp(protocol, "synchronous")) {
    shell->mEmitHeader = PR_TRUE;
    shell->mPrompt = NS_LITERAL_CSTRING("\n> ");
    shell->mProtocol = protocol;
  }
  else if (!strcmp(protocol, "plain")) {
    shell->mEmitHeader = PR_FALSE;
    shell->mPrompt = NS_LITERAL_CSTRING("\n");
    shell->mProtocol = protocol;
  }
  else return JS_FALSE;
  
  return JS_TRUE;
}

static JSBool
GetProtocol(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsJSSh* shell;
  if (!GetJSShGlobal(cx, obj, &shell)) return JS_FALSE;

  JSAutoRequest ar(cx);

  JSString *str = JS_NewStringCopyZ(cx, shell->mProtocol.get());
  *rval = STRING_TO_JSVAL(str);
  return JS_TRUE;
}

static JSBool
SetContextObj(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsJSSh* shell;
  if (!GetJSShGlobal(cx, obj, &shell)) return JS_FALSE;

  JSAutoRequest ar(cx);

  if (argc!=1) return JS_FALSE;

  JSObject *arg_obj;
  if (!JS_ValueToObject(cx, argv[0], &arg_obj)) {
    return JS_FALSE;
  }

  if (shell->mContextObj != shell->mGlobal)
    JS_RemoveRoot(cx, &(shell->mContextObj));
  
  shell->mContextObj = arg_obj;
  
  if (shell->mContextObj != shell->mGlobal)
    JS_AddRoot(cx, &(shell->mContextObj));
  
  return JS_TRUE;
}

static JSBool
DebugBreak(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsJSSh* shell;
  if (!GetJSShGlobal(cx, obj, &shell)) return JS_FALSE;

  NS_BREAK();
  
  return JS_TRUE;
}

static JSBool
GetInputStream(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsJSSh* shell;
  if (!GetJSShGlobal(cx, obj, &shell)) return JS_FALSE;

  JSAutoRequest ar(cx);

  nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());
  if (!xpc) {
    NS_ERROR("failed to get xpconnect service");
    return JS_FALSE;
  }
  
  nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
  xpc->WrapNative(cx, shell->mGlobal, shell->mInput,
                  NS_GET_IID(nsIInputStream),
                  getter_AddRefs(wrapper));
  if (!wrapper) {
    NS_ERROR("could not wrap input stream object");
    return JS_FALSE;
  }

  JSObject* wrapper_jsobj = nsnull;
  wrapper->GetJSObject(&wrapper_jsobj);
  NS_ASSERTION(wrapper_jsobj, "could not get jsobject of wrapped native");

  *rval = OBJECT_TO_JSVAL(wrapper_jsobj);
  
  return JS_TRUE;
}

static JSBool
GetOutputStream(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsJSSh* shell;
  if (!GetJSShGlobal(cx, obj, &shell)) return JS_FALSE;

  JSAutoRequest ar(cx);

  nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());
  if (!xpc) {
    NS_ERROR("failed to get xpconnect service");
    return JS_FALSE;
  }
  
  nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
  xpc->WrapNative(cx, shell->mGlobal, shell->mOutput,
                  NS_GET_IID(nsIOutputStream),
                  getter_AddRefs(wrapper));
  if (!wrapper) {
    NS_ERROR("could not wrap output stream object");
    return JS_FALSE;
  }

  JSObject* wrapper_jsobj = nsnull;
  wrapper->GetJSObject(&wrapper_jsobj);
  NS_ASSERTION(wrapper_jsobj, "could not get jsobject of wrapped native");

  *rval = OBJECT_TO_JSVAL(wrapper_jsobj);
  
  return JS_TRUE;
}

// these all need JSFUN_BOUND_METHOD flags, so that we can do
// things like:
//   win.p = print, where win is rooted in some other global
//                  object.
static JSFunctionSpec global_functions[] = {
  {"print",           Print,          1, JSFUN_BOUND_METHOD, 0},
  {"dump",            Print,          1, JSFUN_BOUND_METHOD, 0},
  {"quit",            Quit,           0, JSFUN_BOUND_METHOD, 0},
  {"exit",            Quit,           0, JSFUN_BOUND_METHOD, 0},
  {"load",            Load,           1, JSFUN_BOUND_METHOD, 0},
  {"suspend",         Suspend,        0, JSFUN_BOUND_METHOD, 0},
  {"resume",          Resume,         0, JSFUN_BOUND_METHOD, 0},
  {"flushEventQueue", FlushEventQueue,0, JSFUN_BOUND_METHOD, 0},
  {"addressOf",       AddressOf,      1, 0,                  0},
  {"setProtocol",     SetProtocol,    1, JSFUN_BOUND_METHOD, 0},
  {"getProtocol",     GetProtocol,    0, JSFUN_BOUND_METHOD, 0},
  {"setContextObj",   SetContextObj,  1, JSFUN_BOUND_METHOD, 0},
  {"debugBreak",      DebugBreak,     0, JSFUN_BOUND_METHOD, 0},
  {"getInputStream",  GetInputStream, 0, JSFUN_BOUND_METHOD, 0},
  {"getOutputStream", GetOutputStream,0, JSFUN_BOUND_METHOD, 0},
  {nsnull, nsnull, 0, 0, 0}
};


//**********************************************************************
// nsJSSh Implementation

nsJSSh::nsJSSh(nsIInputStream* input,
               nsIOutputStream*output,
               const nsACString &startupURI) :
    mInput(input), mOutput(output), mQuit(PR_FALSE), mStartupURI(startupURI),
    mSuspendCount(0), mPrompt("\n> "),
    mEmitHeader(PR_FALSE), mProtocol("interactive")
{
}

nsJSSh::~nsJSSh()
{
  LOG(("JSSh: ~connection!\n"));
}

already_AddRefed<nsIRunnable>
CreateJSSh(nsIInputStream* input, nsIOutputStream*output,
           const nsACString &startupURI)
{
  nsIRunnable* obj = new nsJSSh(input, output, startupURI);
  NS_IF_ADDREF(obj);
  return obj;
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_THREADSAFE_ADDREF(nsJSSh)
NS_IMPL_THREADSAFE_RELEASE(nsJSSh)

NS_INTERFACE_MAP_BEGIN(nsJSSh)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIJSSh)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
  NS_INTERFACE_MAP_ENTRY(nsIJSSh)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
NS_INTERFACE_MAP_END


//----------------------------------------------------------------------
// nsIRunnable methods:

NS_IMETHODIMP nsJSSh::Run()
{
  nsCOMPtr<nsIJSSh> proxied_shell;
  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIProxyObjectManager> pom = do_GetService(NS_XPCOMPROXY_CONTRACTID);
    NS_ASSERTION(pom, "uh-oh, no proxy object manager!");
    pom->GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                           NS_GET_IID(nsIJSSh),
                           (nsIJSSh*)this,
                           NS_PROXY_SYNC,
                           getter_AddRefs(proxied_shell));
  }
  else {
    LOG(("jssh shell will block main thread!\n"));
    proxied_shell = this;
  }
  proxied_shell->Init();

  if (mInput) {
    // read-eval-print loop
    PRUint32 bytesWritten;
    if (mOutput && !mProtocol.Equals(NS_LITERAL_CSTRING("plain")))
      mOutput->Write(gWelcome, strlen(gWelcome), &bytesWritten);
    
    while (!mQuit) {
      if (mOutput)
        mOutput->Write(mPrompt.get(), mPrompt.Length(), &bytesWritten);
      
      // accumulate input until we get a compilable unit:
      PRBool iscompilable;
      mBufferPtr = 0;
#ifdef DEBUG
//     nsCOMPtr<nsIThread> thread;
//     nsIThread::GetCurrent(getter_AddRefs(thread));
//     printf("blocking on thread %p\n", thread.get());
#endif
      do {
        PRUint32 bytesRead = 0;
        mInput->Read(mBuffer+mBufferPtr, 1, &bytesRead);
        if (bytesRead) {
          ++mBufferPtr;
        }
        else {
          // connection was terminated by the client
          mQuit = PR_TRUE;
          break;
        }
        // XXX signal buffer overflow ??
        // XXX ideally we want a dynamically resizing buffer.
      }while (mBufferPtr<cBufferSize &&
              (mBuffer[mBufferPtr-1]!=10 || (NS_SUCCEEDED(proxied_shell->IsBufferCompilable(&iscompilable)) && !iscompilable)));
      NS_ASSERTION(mBufferPtr<cBufferSize, "buffer overflow");
      
      proxied_shell->ExecuteBuffer();
    }
  }
  
  proxied_shell->Cleanup();

  if (!NS_IsMainThread()) {
    // Shutdown the current thread, which must be done from the main thread.
    nsCOMPtr<nsIThread> thread = do_GetCurrentThread();
    nsCOMPtr<nsIThread> proxied_thread;
    nsCOMPtr<nsIProxyObjectManager> pom = do_GetService(NS_XPCOMPROXY_CONTRACTID);
    NS_ASSERTION(pom, "uh-oh, no proxy object manager!");
    pom->GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                           NS_GET_IID(nsIThread),
                           thread.get(),
                           NS_PROXY_ASYNC,
                           getter_AddRefs(proxied_thread));
    if (proxied_thread)
      proxied_thread->Shutdown();
  }
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIJSSh methods

NS_IMETHODIMP nsJSSh::Init()
{
  nsCOMPtr<nsIScriptSecurityManager> ssm = do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
  if (!ssm) {
    NS_ERROR("failed to get script security manager");
    return NS_ERROR_FAILURE;
  }

  ssm->GetSystemPrincipal(getter_AddRefs(mPrincipal));
  if (!mPrincipal) {
    NS_ERROR("failed to get system principal");
    return NS_ERROR_FAILURE;
  }
  
  nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());
  if (!xpc) {
    NS_ERROR("failed to get xpconnect service");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIJSRuntimeService> rtsvc = do_GetService("@mozilla.org/js/xpc/RuntimeService;1");
  // get the JSRuntime from the runtime svc
  if (!rtsvc) {
    NS_ERROR("failed to get nsJSRuntimeService");
    return NS_ERROR_FAILURE;
  }
  
  JSRuntime *rt = nsnull;
  if (NS_FAILED(rtsvc->GetRuntime(&rt)) || !rt) {
    NS_ERROR("failed to get JSRuntime from nsJSRuntimeService");
    return NS_ERROR_FAILURE;
  }
  
  mJSContext = JS_NewContext(rt, 8192);
  if (!mJSContext) {
    NS_ERROR("JS_NewContext failed");
    return NS_ERROR_FAILURE;
  }

  JSAutoRequest ar(mJSContext);

  // Enable e4x:
  JS_SetOptions(mJSContext, JS_GetOptions(mJSContext) | JSOPTION_XML);
  
  // Always use the latest js version
  JS_SetVersion(mJSContext, JSVERSION_LATEST);
  
  mContextStack = do_GetService("@mozilla.org/js/xpc/ContextStack;1");
  if (!mContextStack) {
    NS_ERROR("failed to get the nsThreadJSContextStack service");
    return NS_ERROR_FAILURE;
  }
  
  JS_SetErrorReporter(mJSContext, my_ErrorReporter);

  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
  xpc->InitClassesWithNewWrappedGlobal(mJSContext, (nsIJSSh*)this,
                                       NS_GET_IID(nsISupports),
                                       mPrincipal, nsnull, PR_TRUE,
                                       getter_AddRefs(holder));
  if (!holder) {
    NS_ERROR("global initialization failed");
    return NS_ERROR_FAILURE;
  }
  
  holder->GetJSObject(&mGlobal);
  if (!mGlobal) {
    NS_ERROR("bad global object");
    return NS_ERROR_FAILURE;
  }
  mContextObj = mGlobal;
  
  if (!JS_DefineFunctions(mJSContext, mGlobal, global_functions)) {
    NS_ERROR("failed to initialise global functions");
    return NS_ERROR_FAILURE;
  }
  
  if (!mStartupURI.IsEmpty())
    LoadURL(mStartupURI.get());
  
  return NS_OK;
}

NS_IMETHODIMP nsJSSh::Cleanup()
{
  nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());
  if (!xpc) {
    NS_ERROR("failed to get xpconnect service");
    return NS_ERROR_FAILURE;
  }

  {
    JSAutoRequest ar(mJSContext);

    if (mContextObj != mGlobal)
      JS_RemoveRoot(mJSContext, &(mContextObj));

    JS_ClearScope(mJSContext, mGlobal);
    JS_GC(mJSContext);
  }

  JS_DestroyContext(mJSContext);
  return NS_OK;
}

NS_IMETHODIMP nsJSSh::ExecuteBuffer()
{
#ifdef DEBUG
//     nsCOMPtr<nsIThread> thread;
//     nsIThread::GetCurrent(getter_AddRefs(thread));
//     printf("executing on thread %p\n", thread.get());
#endif

  JS_BeginRequest(mJSContext);
  JS_ClearPendingException(mJSContext);
  JSPrincipals *jsprincipals;
  mPrincipal->GetJSPrincipals(mJSContext, &jsprincipals);

  if(NS_FAILED(mContextStack->Push(mJSContext))) {
    NS_ERROR("failed to push the current JSContext on the nsThreadJSContextStack");
    return NS_ERROR_FAILURE;
  }
  
  JSScript *script = JS_CompileScriptForPrincipals(mJSContext, mContextObj, jsprincipals, mBuffer, mBufferPtr, "interactive", 0);

  if (script) {
    jsval result;
    if (JS_ExecuteScript(mJSContext, mContextObj, script, &result) && result!=JSVAL_VOID && mOutput) {
      // XXX for some wrapped native objects the following code will crash; probably because the
      // native object is getting released before we reach this:
       JSString *str = JS_ValueToString(mJSContext, result);
       if (str) {
         nsDependentString chars(reinterpret_cast<const PRUnichar*>
                                 (JS_GetStringChars(str)),
                                 JS_GetStringLength(str));
         NS_ConvertUTF16toUTF8 cstr(chars);
         PRUint32 bytesWritten;
         mOutput->Write(cstr.get(), cstr.Length(), &bytesWritten);
       }
    }
    JS_DestroyScript(mJSContext, script);
  }

  JSContext *oldcx;
  mContextStack->Pop(&oldcx);
  NS_ASSERTION(oldcx == mJSContext, "JS thread context push/pop mismatch");

  JSPRINCIPALS_DROP(mJSContext, jsprincipals);
  
  JS_EndRequest(mJSContext);
  
  return NS_OK;
}

NS_IMETHODIMP nsJSSh::IsBufferCompilable(PRBool *_retval)
{
  JSAutoRequest ar(mJSContext);
  *_retval = JS_BufferIsCompilableUnit(mJSContext, mContextObj, mBuffer, mBufferPtr);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIScriptObjectPrincipal methods:

nsIPrincipal *
nsJSSh::GetPrincipal()
{
  return mPrincipal;
}

//----------------------------------------------------------------------
// nsIXPCScriptable methods:

#define XPC_MAP_CLASSNAME           nsJSSh
#define XPC_MAP_QUOTED_CLASSNAME   "JSSh"
#define                             XPC_MAP_WANT_NEWRESOLVE
#define XPC_MAP_FLAGS       nsIXPCScriptable::USE_JSSTUB_FOR_ADDPROPERTY   | \
                            nsIXPCScriptable::USE_JSSTUB_FOR_DELPROPERTY   | \
                            nsIXPCScriptable::USE_JSSTUB_FOR_SETPROPERTY   | \
                            nsIXPCScriptable::DONT_ENUM_STATIC_PROPS       | \
                            nsIXPCScriptable::DONT_ENUM_QUERY_INTERFACE    | \
                            nsIXPCScriptable::DONT_REFLECT_INTERFACE_NAMES
#include "xpc_map_end.h" /* This will #undef the above */

/* PRBool newResolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx,
   in JSObjectPtr obj, in jsval id, in PRUint32 flags, out JSObjectPtr objp); */
NS_IMETHODIMP
nsJSSh::NewResolve(nsIXPConnectWrappedNative *wrapper,
                   JSContext * cx, JSObject * obj,
                   jsval id, PRUint32 flags, 
                   JSObject * *objp, PRBool *_retval)
{
  JSBool resolved;
  
  JSAutoRequest ar(cx);

  *_retval = JS_ResolveStandardClass(cx, obj, id, &resolved);
  if (*_retval && resolved)
    *objp = obj;
  return NS_OK;
}

//----------------------------------------------------------------------
// Implementation helpers:

PRBool nsJSSh::LoadURL(const char *url, jsval* retval)
{
  nsCOMPtr<nsIIOService> ioserv = do_GetService(NS_IOSERVICE_CONTRACTID);
  if (!ioserv) {
    NS_ERROR("could not get io service");
    return PR_FALSE;
  }

  nsCOMPtr<nsIChannel> channel;
  ioserv->NewChannel(nsDependentCString(url), nsnull, nsnull, getter_AddRefs(channel));
  if (!channel) {
    NS_ERROR("could not create channel");
    return PR_FALSE;
  }
  
  nsCOMPtr<nsIInputStream> instream;
  channel->Open(getter_AddRefs(instream));
  if (!instream) {
    NS_ERROR("could not open stream");
    return PR_FALSE;
  }

  nsCString buffer;
  nsAutoArrayPtr<char> buf(new char[1024]);
  if (!buf) {
    NS_ERROR("could not alloc buffer");
    return PR_FALSE;
  }

  PRUint32 bytesRead = 0;

  do {
    if (NS_FAILED(instream->Read(buf, 1024, &bytesRead))) {
      NS_ERROR("stream read error");
      return PR_FALSE;
    }
    buffer.Append(buf, bytesRead);
    LOG(("appended %d bytes:\n%s", bytesRead, buffer.get()));
  } while (bytesRead > 0);

  LOG(("loaded %d bytes:\n%s", buffer.Length(), buffer.get()));

  JS_BeginRequest(mJSContext);
  JSPrincipals *jsprincipals;
  mPrincipal->GetJSPrincipals(mJSContext, &jsprincipals);

  if(NS_FAILED(mContextStack->Push(mJSContext))) {
    NS_ERROR("failed to push the current JSContext on the nsThreadJSContextStack");
    return NS_ERROR_FAILURE;
  }

  jsval result;
  JSBool ok = JS_EvaluateScriptForPrincipals(mJSContext, mContextObj,
                                             jsprincipals, buffer.get(),
                                             buffer.Length(),
                                             url, 1, &result);
  JSPRINCIPALS_DROP(mJSContext, jsprincipals);

  JSContext *oldcx;
  mContextStack->Pop(&oldcx);
  NS_ASSERTION(oldcx == mJSContext, "JS thread context push/pop mismatch");

  if (ok && retval) *retval=result;
  
  JS_EndRequest(mJSContext);

  return ok;
}
  
