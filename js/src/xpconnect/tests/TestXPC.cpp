/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/* Tests for XPConnect - hacked together while development continues. */

#include <stdio.h>

#include "nsIXPConnect.h"
#include "nsIXPCScriptable.h"
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIXPCScriptable.h"
#include "nsIXPCSecurityManager.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "jsapi.h"
#include "xpclog.h"
#include "nscore.h"

#include "xpctest.h"

#include "nsIAllocator.h"
#include "nsIGenericFactory.h"
#include "nsSpecialSystemDirectory.h"	// For exe dir

#include "nsIJSContextStack.h"

static NS_DEFINE_CID(kGenericFactoryCID, NS_GENERICFACTORY_CID);
static NS_DEFINE_IID(kIAllocatorIID, NS_IALLOCATOR_IID);
static NS_DEFINE_IID(kAllocatorCID, NS_ALLOCATOR_CID);

static void SetupRegistry()
{
    nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, nsnull);
}


/***************************************************************************/
class MyScriptable : public nsIXPCScriptable
{
    NS_DECL_ISUPPORTS
    XPC_DECLARE_IXPCSCRIPTABLE
    MyScriptable();
};

MyScriptable::MyScriptable()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

static NS_DEFINE_IID(kMyScriptableIID, NS_IXPCSCRIPTABLE_IID);
NS_IMPL_ISUPPORTS(MyScriptable, kMyScriptableIID);

// XPC_IMPLEMENT_FORWARD_IXPCSCRIPTABLE(MyScriptable);
    XPC_IMPLEMENT_FORWARD_CREATE(MyScriptable);
    XPC_IMPLEMENT_IGNORE_GETFLAGS(MyScriptable);
    XPC_IMPLEMENT_FORWARD_LOOKUPPROPERTY(MyScriptable);
    XPC_IMPLEMENT_FORWARD_DEFINEPROPERTY(MyScriptable);
    XPC_IMPLEMENT_FORWARD_GETPROPERTY(MyScriptable);
    XPC_IMPLEMENT_FORWARD_SETPROPERTY(MyScriptable);
    XPC_IMPLEMENT_FORWARD_GETATTRIBUTES(MyScriptable);
    XPC_IMPLEMENT_FORWARD_SETATTRIBUTES(MyScriptable);
    XPC_IMPLEMENT_FORWARD_DELETEPROPERTY(MyScriptable);
//    XPC_IMPLEMENT_FORWARD_DEFAULTVALUE(MyScriptable);
    XPC_IMPLEMENT_FORWARD_ENUMERATE(MyScriptable);
    XPC_IMPLEMENT_FORWARD_CHECKACCESS(MyScriptable);
    XPC_IMPLEMENT_FORWARD_CALL(MyScriptable);
    XPC_IMPLEMENT_FORWARD_CONSTRUCT(MyScriptable);
    XPC_IMPLEMENT_FORWARD_FINALIZE(MyScriptable);

NS_IMETHODIMP
MyScriptable::DefaultValue(JSContext *cx, JSObject *obj,
                            JSType type, jsval *vp,
                            nsIXPConnectWrappedNative* wrapper,
                            nsIXPCScriptable* arbitrary,
                            JSBool* retval)
{
    if(type == JSTYPE_STRING || type == JSTYPE_VOID)
    {
        *vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, "obj with MyScriptable"));
        *retval = JS_TRUE;
        return NS_OK;
    }
    return arbitrary->DefaultValue(cx, obj, type, vp, wrapper, NULL, retval);
}

/***************************************************************************/

class nsTestXPCFoo : public nsITestXPCFoo2
{
    NS_DECL_ISUPPORTS
    NS_IMETHOD Test(int p1, int p2, int* retval);
    NS_IMETHOD Test2();

    /* attribute string Foo; */
    NS_IMETHOD GetFoo(char * *aFoo);
    NS_IMETHOD SetFoo(char * aFoo);

    nsTestXPCFoo();
    virtual ~nsTestXPCFoo();
    char* mFoo;
};

NS_IMETHODIMP nsTestXPCFoo::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  *aInstancePtr = NULL;

  if (aIID.Equals(nsITestXPCFoo::GetIID()) ||
      aIID.Equals(nsITestXPCFoo2::GetIID()) ||
      aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
    *aInstancePtr = (void*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIXPCScriptable::GetIID())) {
    *aInstancePtr = (void*) new MyScriptable();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

NS_IMETHODIMP nsTestXPCFoo::Test(int p1, int p2, int* retval)
{
    printf("nsTestXPCFoo::Test called with p1 = %d and p2 = %d\n", p1, p2);
    *retval = p1+p2;
    return NS_OK;
}
NS_IMETHODIMP nsTestXPCFoo::Test2()
{
    printf("nsTestXPCFoo::Test2 called ");
    return NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::GetFoo(char * *aFoo)
{
    printf("nsTestXPCFoo::Get called ");
    if(!aFoo)
        return NS_ERROR_NULL_POINTER;
    if(mFoo)
        *aFoo = (char*) nsAllocator::Clone(mFoo, strlen(mFoo)+1);
    else
        *aFoo = NULL;
    return NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::SetFoo(char * aFoo)
{
    printf("nsTestXPCFoo::Set called ");
    if(mFoo)
    {
        nsAllocator::Free(mFoo);
        mFoo = NULL;
    }
    if(aFoo)
        mFoo = (char*) nsAllocator::Clone(aFoo, strlen(aFoo)+1);
    return NS_OK;
}

NS_IMPL_ADDREF(nsTestXPCFoo)
NS_IMPL_RELEASE(nsTestXPCFoo)

nsTestXPCFoo::nsTestXPCFoo()
    : mFoo(NULL)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

nsTestXPCFoo::~nsTestXPCFoo()
{
    if(mFoo)
        nsAllocator::Free(mFoo);
}


/***************************************************************************/

class MyEcho : public nsIEcho
{
public:
    NS_DECL_ISUPPORTS
    NS_IMETHOD SetReceiver(nsIEcho* aReceiver);
    NS_IMETHOD SendOneString(const char* str);
    NS_IMETHOD In2OutOneInt(int input, int* output);
    NS_IMETHOD In2OutAddTwoInts(int input1,
                                int input2,
                                int* output1,
                                int* output2,
                                int* result);
    NS_IMETHOD In2OutOneString(const char* input, char** output);
    NS_IMETHOD SimpleCallNoEcho();
    NS_IMETHOD SendManyTypes(PRUint8              p1,
                             PRInt16             p2,
                             PRInt32             p3,
                             PRInt64             p4,
                             PRUint8              p5,
                             PRUint16            p6,
                             PRUint32            p7,
                             PRUint64            p8,
                             float             p9,
                             double            p10,
                             PRBool            p11,
                             char              p12,
                             PRUnichar         p13,
                             const nsID*       p14,
                             const char*       p15,
                             const PRUnichar*  p16);
    NS_IMETHOD SendInOutManyTypes(PRUint8*    p1,
                                  PRInt16*   p2,
                                  PRInt32*   p3,
                                  PRInt64*   p4,
                                  PRUint8*    p5,
                                  PRUint16*  p6,
                                  PRUint32*  p7,
                                  PRUint64*  p8,
                                  float*   p9,
                                  double*  p10,
                                  PRBool*  p11,
                                  char*    p12,
                                  PRUnichar*  p13,
                                  nsID**   p14,
                                  char**   p15,
                                  PRUnichar** p16);
    NS_IMETHOD MethodWithNative(int p1, void* p2);

    NS_IMETHOD ReturnCode(int code);

    NS_IMETHOD FailInJSTest(int fail);

    /* void SharedTest ([shared, retval] out string str); */
    NS_IMETHOD SharedString(const char **str);

    /* void ReturnCode_NS_OK (); */
    NS_IMETHOD ReturnCode_NS_OK();

    /* void ReturnCode_NS_COMFALSE (); */
    NS_IMETHOD ReturnCode_NS_COMFALSE();

    /* void ReturnCode_NS_ERROR_NULL_POINTER (); */
    NS_IMETHOD ReturnCode_NS_ERROR_NULL_POINTER();

    /* void ReturnCode_NS_ERROR_UNEXPECTED (); */
    NS_IMETHOD ReturnCode_NS_ERROR_UNEXPECTED();

    /* void ReturnCode_NS_ERROR_OUT_OF_MEMORY (); */
    NS_IMETHOD ReturnCode_NS_ERROR_OUT_OF_MEMORY();

    /* nsISupports ReturnInterface (in nsISupports obj); */
    NS_IMETHOD ReturnInterface(nsISupports *obj, nsISupports **_retval);

    /* nsIJSStackFrameLocation GetStack (); */
    NS_IMETHOD GetStack(nsIJSStackFrameLocation **_retval);

    /* void SetReceiverReturnOldReceiver (inout nsIEcho aReceiver); */
    NS_IMETHOD SetReceiverReturnOldReceiver(nsIEcho **aReceiver);

    /* void MethodWithForwardDeclaredParam (in nsITestXPCSomeUselessThing sut); */
    NS_IMETHOD MethodWithForwardDeclaredParam(nsITestXPCSomeUselessThing *sut);

    MyEcho();
private:
    nsIEcho* mReceiver;
    nsIAllocator* mAllocator;
};

static NS_DEFINE_IID(kMyEchoIID, NS_IECHO_IID);
NS_IMPL_ISUPPORTS(MyEcho, kMyEchoIID);

MyEcho::MyEcho()
    : mReceiver(NULL)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
    nsServiceManager::GetService(kAllocatorCID,
                                 kIAllocatorIID,
                                 (nsISupports **)&mAllocator);
}

NS_IMETHODIMP MyEcho::SetReceiver(nsIEcho* aReceiver)
{
    if(mReceiver)
        NS_RELEASE(mReceiver);
    mReceiver = aReceiver;
    if(mReceiver)
        NS_ADDREF(mReceiver);
    return NS_OK;
}

NS_IMETHODIMP MyEcho::SendOneString(const char* str)
{
    if(mReceiver)
        return mReceiver->SendOneString(str);
    return NS_OK;
}

NS_IMETHODIMP MyEcho::In2OutOneInt(int input, int* output)
{
    *output = input;
    return NS_OK;
}

NS_IMETHODIMP MyEcho::In2OutAddTwoInts(int input1,
                                       int input2,
                                       int* output1,
                                       int* output2,
                                       int* result)
{
    *output1 = input1;
    *output2 = input2;
    *result = input1+input2;
    return NS_OK;
}

NS_IMETHODIMP MyEcho::In2OutOneString(const char* input, char** output)
{
    char* p;
    int len;
    if(input && output && mAllocator &&
       (NULL != (p = (char*)mAllocator->Alloc(len=strlen(input)+1))))
    {
        memcpy(p, input, len);
        *output = p;
        return NS_OK;
    }
    if(output)
        *output = NULL;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP MyEcho::SimpleCallNoEcho()
{
    return NS_OK;
}

NS_IMETHODIMP
MyEcho::SendManyTypes(PRUint8              p1,
                      PRInt16             p2,
                      PRInt32             p3,
                      PRInt64             p4,
                      PRUint8              p5,
                      PRUint16            p6,
                      PRUint32            p7,
                      PRUint64            p8,
                      float             p9,
                      double            p10,
                      PRBool            p11,
                      char              p12,
                      PRUnichar            p13,
                      const nsID*       p14,
                      const char*       p15,
                      const PRUnichar*  p16)
{
    if(mReceiver)
        return mReceiver->SendManyTypes(p1, p2, p3, p4, p5, p6, p7, p8, p9,
                                        p10, p11, p12, p13, p14, p15, p16);
    return NS_OK;
}

NS_IMETHODIMP
MyEcho::SendInOutManyTypes(PRUint8*    p1,
                           PRInt16*   p2,
                           PRInt32*   p3,
                           PRInt64*   p4,
                           PRUint8*    p5,
                           PRUint16*  p6,
                           PRUint32*  p7,
                           PRUint64*  p8,
                           float*   p9,
                           double*  p10,
                           PRBool*  p11,
                           char*    p12,
                           PRUnichar*  p13,
                           nsID**   p14,
                           char**   p15,
                           PRUnichar** p16)
{
    if(mReceiver)
        return mReceiver->SendInOutManyTypes(p1, p2, p3, p4, p5, p6, p7, p8, p9,
                                             p10, p11, p12, p13, p14, p15, p16);
    return NS_OK;
}

NS_IMETHODIMP
MyEcho::MethodWithNative(int p1, void* p2)
{
    return NS_OK;
}

NS_IMETHODIMP
MyEcho::ReturnCode(int code)
{
    return (nsresult) code;
}

NS_IMETHODIMP
MyEcho::FailInJSTest(int fail)
{
    if(mReceiver)
        return mReceiver->FailInJSTest(fail);
    return NS_OK;
}

NS_IMETHODIMP
MyEcho::SharedString(const char **str)
{
    *str = "a static string";
/*
    // to do non-shared we clone the string:
    char buf[] = "a static string";
    int len;
    *str = (char*)mAllocator->Alloc(len=strlen(buf)+1);
    memcpy(*str, buf, len);
*/
    return NS_OK;
}

NS_IMETHODIMP
MyEcho::ReturnCode_NS_OK()
{return NS_OK;}

NS_IMETHODIMP
MyEcho::ReturnCode_NS_COMFALSE()
{return NS_COMFALSE;}

NS_IMETHODIMP
MyEcho::ReturnCode_NS_ERROR_NULL_POINTER()
{return NS_ERROR_NULL_POINTER;}

NS_IMETHODIMP
MyEcho::ReturnCode_NS_ERROR_UNEXPECTED()
{return NS_ERROR_UNEXPECTED;}

NS_IMETHODIMP
MyEcho::ReturnCode_NS_ERROR_OUT_OF_MEMORY()
{return NS_ERROR_OUT_OF_MEMORY;}

NS_IMETHODIMP
MyEcho::ReturnInterface(nsISupports *obj, nsISupports **_retval)
{
    if(!_retval)
        return NS_ERROR_NULL_POINTER;
    if(obj)
        NS_ADDREF(obj);
    *_retval = obj;
    return NS_OK;
}

/* nsIJSStackFrameLocation GetStack (); */
NS_IMETHODIMP
MyEcho::GetStack(nsIJSStackFrameLocation **_retval)
{
    nsIJSStackFrameLocation* stack = nsnull;
    if(!_retval)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    NS_WITH_SERVICE(nsIXPConnect, xpc, nsIXPConnect::GetCID(), &rv);
    if(NS_SUCCEEDED(rv))
    {
        nsIJSStackFrameLocation* jsstack;
        if(NS_SUCCEEDED(xpc->GetCurrentJSStack(&jsstack)) && jsstack)
        {
            xpc->CreateStackFrameLocation(JS_FALSE,
                                          __FILE__,
                                          "MyEcho::GetStack",
                                          __LINE__,
                                          jsstack,
                                          &stack);
            NS_RELEASE(jsstack);
        }
    }

    if(stack)
    {
        *_retval = stack;
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

/* void SetReceiverReturnOldReceiver (inout nsIEcho aReceiver); */
NS_IMETHODIMP
MyEcho::SetReceiverReturnOldReceiver(nsIEcho **aReceiver)
{
    if(!aReceiver)
        return NS_ERROR_NULL_POINTER;

    nsIEcho* oldReceiver = mReceiver;
    mReceiver = *aReceiver;
    if(mReceiver)
        NS_ADDREF(mReceiver);

    /* don't release the reference, that is the caller's problem */
    *aReceiver = oldReceiver;
    return NS_OK;
}

/* void MethodWithForwardDeclaredParam (in nsITestXPCSomeUselessThing sut); */
NS_IMETHODIMP
MyEcho::MethodWithForwardDeclaredParam(nsITestXPCSomeUselessThing *sut)
{
    return NS_OK;
}

/***************************************************************************/
// security manager test class

class MySecMan : public nsIXPCSecurityManager
{
public:
  NS_DECL_ISUPPORTS

  enum Mode { OK_ALL    ,
              VETO_ALL
            };

  /* void CanCreateWrapper (in JSContext aJSContext, in nsIIDRef aIID, in nsISupports aObj); */
  NS_IMETHOD CanCreateWrapper(JSContext * aJSContext, const nsIID & aIID, nsISupports *aObj);

  /* void CanCreateInstance (in JSContext aJSContext, in nsCIDRef aCID); */
  NS_IMETHOD CanCreateInstance(JSContext * aJSContext, const nsCID & aCID);

  /* void CanGetService (in JSContext aJSContext, in nsCIDRef aCID); */
  NS_IMETHOD CanGetService(JSContext * aJSContext, const nsCID & aCID);

  /* void CanCallMethod (in JSContext aJSContext, in nsIIDRef aIID, in nsISupports aObj, in nsIInterfaceInfo aInterfaceInfo, in PRUint16 aMethodIndex, [const] in jsid aName); */
  NS_IMETHOD CanCallMethod(JSContext * aJSContext, const nsIID & aIID, nsISupports *aObj, nsIInterfaceInfo *aInterfaceInfo, PRUint16 aMethodIndex, const jsid aName);

  /* void CanGetProperty (in JSContext aJSContext, in nsIIDRef aIID, in nsISupports aObj, in nsIInterfaceInfo aInterfaceInfo, in PRUint16 aMethodIndex, [const] in jsid aName); */
  NS_IMETHOD CanGetProperty(JSContext * aJSContext, const nsIID & aIID, nsISupports *aObj, nsIInterfaceInfo *aInterfaceInfo, PRUint16 aMethodIndex, const jsid aName);

  /* void CanSetProperty (in JSContext aJSContext, in nsIIDRef aIID, in nsISupports aObj, in nsIInterfaceInfo aInterfaceInfo, in PRUint16 aMethodIndex, [const] in jsid aName); */
  NS_IMETHOD CanSetProperty(JSContext * aJSContext, const nsIID & aIID, nsISupports *aObj, nsIInterfaceInfo *aInterfaceInfo, PRUint16 aMethodIndex, const jsid aName);

  void SetMode(Mode mode) {mMode = mode;}

  MySecMan();

private:
  Mode mMode;
};

static NS_DEFINE_IID(kMySecManIID, NS_IXPCSECURITYMANAGER_IID);
NS_IMPL_ISUPPORTS(MySecMan, kMySecManIID);

MySecMan::MySecMan()
    : mMode(OK_ALL)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

NS_IMETHODIMP
MySecMan::CanCreateWrapper(JSContext * aJSContext, const nsIID & aIID, nsISupports *aObj)
{
    switch(mMode)
    {
        case OK_ALL:
            return NS_OK;
        case VETO_ALL:
            JS_SetPendingException(aJSContext,
                STRING_TO_JSVAL(JS_NewStringCopyZ(aJSContext,
                    "security exception")));
            return NS_ERROR_FAILURE;
        default:
            NS_ASSERTION(0,"bad case");
            return NS_OK;
    }
}

NS_IMETHODIMP
MySecMan::CanCreateInstance(JSContext * aJSContext, const nsCID & aCID)
{
    switch(mMode)
    {
        case OK_ALL:
            return NS_OK;
        case VETO_ALL:
            JS_SetPendingException(aJSContext,
                STRING_TO_JSVAL(JS_NewStringCopyZ(aJSContext,
                    "security exception")));
            return NS_ERROR_FAILURE;
        default:
            NS_ASSERTION(0,"bad case");
            return NS_OK;
    }
}

NS_IMETHODIMP
MySecMan::CanGetService(JSContext * aJSContext, const nsCID & aCID)
{
    switch(mMode)
    {
        case OK_ALL:
            return NS_OK;
        case VETO_ALL:
            JS_SetPendingException(aJSContext,
                STRING_TO_JSVAL(JS_NewStringCopyZ(aJSContext,
                    "security exception")));
            return NS_ERROR_FAILURE;
        default:
            NS_ASSERTION(0,"bad case");
            return NS_OK;
    }
}

NS_IMETHODIMP
MySecMan::CanCallMethod(JSContext * aJSContext, const nsIID & aIID, nsISupports *aObj, nsIInterfaceInfo *aInterfaceInfo, PRUint16 aMethodIndex, const jsid aName)
{
    switch(mMode)
    {
        case OK_ALL:
            return NS_OK;
        case VETO_ALL:
            JS_SetPendingException(aJSContext,
                STRING_TO_JSVAL(JS_NewStringCopyZ(aJSContext,
                    "security exception")));
            return NS_ERROR_FAILURE;
        default:
            NS_ASSERTION(0,"bad case");
            return NS_OK;
    }
}

NS_IMETHODIMP
MySecMan::CanGetProperty(JSContext * aJSContext, const nsIID & aIID, nsISupports *aObj, nsIInterfaceInfo *aInterfaceInfo, PRUint16 aMethodIndex, const jsid aName)
{
    switch(mMode)
    {
        case OK_ALL:
            return NS_OK;
        case VETO_ALL:
            JS_SetPendingException(aJSContext,
                STRING_TO_JSVAL(JS_NewStringCopyZ(aJSContext,
                    "security exception")));
            return NS_ERROR_FAILURE;
        default:
            NS_ASSERTION(0,"bad case");
            return NS_OK;
    }
}

NS_IMETHODIMP
MySecMan::CanSetProperty(JSContext * aJSContext, const nsIID & aIID, nsISupports *aObj, nsIInterfaceInfo *aInterfaceInfo, PRUint16 aMethodIndex, const jsid aName)
{
    switch(mMode)
    {
        case OK_ALL:
            return NS_OK;
        case VETO_ALL:
            JS_SetPendingException(aJSContext,
                STRING_TO_JSVAL(JS_NewStringCopyZ(aJSContext,
                    "security exception")));
            return NS_ERROR_FAILURE;
        default:
            NS_ASSERTION(0,"bad case");
            return NS_OK;
    }
}

/***************************************************************************/

FILE *gOutFile = NULL;
FILE *gErrFile = NULL;

static JSBool
Print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN i, n;
    JSString *str;

    for (i = n = 0; i < argc; i++) {
        str = JS_ValueToString(cx, argv[i]);
        if (!str)
            return JS_FALSE;
        fprintf(gOutFile, "%s%s", i ? " " : "", JS_GetStringBytes(str));
    }
    n++;
    if (n)
        fputc('\n', gOutFile);
    return JS_TRUE;
}

static JSBool
Load(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN i;
    JSString *str;
    const char *filename;
    JSScript *script;
    JSBool ok;
    jsval result;

    for (i = 0; i < argc; i++) {
        str = JS_ValueToString(cx, argv[i]);
        if (!str)
            return JS_FALSE;
        argv[i] = STRING_TO_JSVAL(str);
        filename = JS_GetStringBytes(str);
        script = JS_CompileFile(cx, obj, filename);
        if (!script)
            ok = JS_FALSE;
        else {
            ok = JS_ExecuteScript(cx, obj, script, &result);
            JS_DestroyScript(cx, script);
        }
        if (!ok)
            return JS_FALSE;
    }
    return JS_TRUE;
}

static JSFunctionSpec glob_functions[] = {
    {"print",           Print,          0},
    {"load",            Load,           1},
    {0}
};

static JSClass global_class = {
    "global", 0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

static void
my_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
    printf(message);
}

static nsIXPConnect* GetXPConnect()
{
    nsIXPConnect* result;

    if(NS_SUCCEEDED(nsServiceManager::GetService(
                        nsIXPConnect::GetCID(), nsIXPConnect::GetIID(),
                        (nsISupports**) &result, NULL)))
        return result;
    return NULL;
}

extern "C" JS_FRIEND_DATA(FILE *) js_DumpGCHeap;

int main()
{
    JSRuntime *rt;
    JSContext *jscontext;
    JSObject *glob;

    gErrFile = stderr;
    gOutFile = stdout;

    SetupRegistry();

    rt = JS_NewRuntime(8L * 1024L * 1024L);
    if (!rt)
        return 1;
    jscontext = JS_NewContext(rt, 8192);
    if (!jscontext)
        return 1;

    JS_SetErrorReporter(jscontext, my_ErrorReporter);

    nsIXPConnect* xpc = GetXPConnect();
    if(!xpc)
    {
        printf("XPC_GetXPConnect() returned NULL!\n");
        return 1;
    }

#if 0

    // old code where global object was plain object
    glob = JS_NewObject(jscontext, &global_class, NULL, NULL);
    if (!glob)
        return 1;

    if (!JS_InitStandardClasses(jscontext, glob))
        return 1;
    if (!JS_DefineFunctions(jscontext, glob, glob_functions))
        return 1;

    xpc->InitJSContext(jscontext, glob, JS_TRUE);

#if 0
    nsIXPCComponents* comp = XPC_GetXPConnectComponentsObject();
    if(!comp)
    {
        printf("failed to create Components native object");
        return 1;
    }
    nsIXPConnectWrappedNative* comp_wrapper;
    if(NS_FAILED(xpc->WrapNative(jscontext, comp,
                              nsIXPCComponents::GetIID(), &comp_wrapper)))
    {
        printf("failed to build wrapper for Components native object");
        return 1;
    }
    JSObject* comp_jsobj;
    comp_wrapper->GetJSObject(&comp_jsobj);
    jsval comp_jsval = OBJECT_TO_JSVAL(comp_jsobj);
    JS_SetProperty(jscontext, glob, "Components", &comp_jsval);
    NS_RELEASE(comp_wrapper);
    NS_RELEASE(comp);
#endif

    char* txt[] = {
      "load('simpletest.js');",
      0,
    };

    jsval rval;
    for(char** p = txt; *p; p++)
        JS_EvaluateScript(jscontext, glob, *p, strlen(*p), "builtin", 1, &rval);

//    XPC_DUMP(xpc, 20);

#else

    nsTestXPCFoo* foo = new nsTestXPCFoo();

//    nsXPCVarient v[2];
//    v[0].type = nsXPCType::T_I32; v[0].val.i32 = 1;
//    v[1].type = nsXPCType::T_I32; v[1].val.i32 = 2;

//    XPC_TestInvoke(foo, 3, 2, v);
//    XPC_TestInvoke(foo, 4, 0, NULL);

    nsIXPConnectWrappedNative* wrapper;
    nsIXPConnectWrappedNative* wrapper2;
    nsIXPConnectWrappedNative* wrapper3;


    nsIXPConnectWrappedNative* fool_wrapper = NULL;

/*
    if(NS_SUCCEEDED(xpc->WrapNative(jscontext, foo, nsITestXPCFoo::GetIID(), &wrapper)))
*/
    // new code where global object is a wrapped xpcom object
    if(NS_SUCCEEDED(xpc->InitJSContextWithNewWrappedGlobal(
                jscontext, foo, nsITestXPCFoo::GetIID(), JS_TRUE, &wrapper)))
    {
        wrapper->GetJSObject(&glob);
        JS_DefineFunctions(jscontext, glob, glob_functions);

#if 0
        nsIXPCComponents* comp = XPC_GetXPConnectComponentsObject();
        if(!comp)
        {
            printf("failed to create Components native object");
            return 1;
        }
        nsIXPConnectWrappedNative* comp_wrapper;
        if(NS_FAILED(xpc->WrapNative(jscontext, comp,
                                  nsIXPCComponents::GetIID(), &comp_wrapper)))
        {
            printf("failed to build wrapper for Components native object");
            return 1;
        }
        JSObject* comp_jsobj;
        comp_wrapper->GetJSObject(&comp_jsobj);
        jsval comp_jsval = OBJECT_TO_JSVAL(comp_jsobj);
        JS_SetProperty(jscontext, glob, "Components", &comp_jsval);
        NS_RELEASE(comp_wrapper);
        NS_RELEASE(comp);
#endif

        nsTestXPCFoo* fool = new nsTestXPCFoo();
        xpc->WrapNative(jscontext, fool, nsITestXPCFoo2::GetIID(), &fool_wrapper);

        if(NS_SUCCEEDED(xpc->WrapNative(jscontext, foo, nsITestXPCFoo2::GetIID(), &wrapper2)))
        {
            JSObject* js_obj;
            nsISupports* com_obj;
            jsval rval;

            wrapper2->GetJSObject(&js_obj);
            wrapper2->GetNative(&com_obj);

            jsval v;
            v = OBJECT_TO_JSVAL(js_obj);
            JS_SetProperty(jscontext, glob, "foo", &v);

            // add the reflected native echo object (sans error checking :)
            nsIXPConnectWrappedNative* echo_wrapper;
            JSObject* echo_jsobj;
            jsval echo_jsval;
            MyEcho* myEcho = new MyEcho();
            xpc->WrapNative(jscontext, myEcho, nsIEcho::GetIID(), &echo_wrapper);
            echo_wrapper->GetJSObject(&echo_jsobj);
            echo_jsval = OBJECT_TO_JSVAL(echo_jsobj);
            JS_SetProperty(jscontext, glob, "echo", &echo_jsval);

            char* txt[] = {
              "load('testxpc.js');",
              0,
            };

            for(char** p = txt; *p; p++)
                JS_EvaluateScript(jscontext, glob, *p, strlen(*p), "builtin", 1, &rval);

            if(JS_GetProperty(jscontext, glob, "bar", &v) && JSVAL_IS_OBJECT(v))
            {
                JSObject* bar = JSVAL_TO_OBJECT(v);
                nsISupports* wrapper3;
                if(NS_SUCCEEDED(xpc->WrapJS(jscontext,
                                       JSVAL_TO_OBJECT(v),
                                       nsITestXPCFoo::GetIID(), &wrapper3)))
                {
                    nsITestXPCFoo* ptr = (nsITestXPCFoo*)wrapper3;
                    int result;
                    JSObject* test_js_obj;
                    ptr->Test(11, 13, &result);
                    printf("call to ptr->Test result: %s\n",
                           result == 24 ? "passed" : "FAILED");

                    nsIXPConnectWrappedJSMethods* methods;

                    wrapper3->QueryInterface(nsIXPConnectWrappedJSMethods::GetIID(),
                                            (void**) &methods);
                    methods->GetJSObject(&test_js_obj);

                    printf("call to methods->GetJSObject() : %s\n",
                            test_js_obj == JSVAL_TO_OBJECT(v) ?
                            "passed" : "FAILED" );

                    char some_string[] = "some string here";
                    char* answer = NULL;
//                    ptr->GetFoo(&answer);
//                    printf("Foo : %s\n", answer);
                    ptr->SetFoo(some_string);
                    ptr->GetFoo(&answer);
                    printf("set/get property : %s\n",
                           0 == strcmp(some_string, answer) ?
                                "passed" : "FALIED");

                    if(answer)
                        nsAllocator::Free(answer);

                    // dump to log test...
//                    XPC_DUMP(xpc, 50);

                    NS_RELEASE(methods);
                    NS_RELEASE(wrapper3);

                }
            }
            NS_RELEASE(com_obj);
            NS_RELEASE(wrapper2);
            NS_RELEASE(echo_wrapper);
            NS_RELEASE(myEcho);
        }
//        NS_RELEASE(wrapper);
    }

    // XXX things are still rooted to the global object in the test scripts...

    // dump to log test...
//    XPC_LOG_ALWAYS((""));
//    XPC_LOG_ALWAYS(("after running release object..."));
//    XPC_LOG_ALWAYS((""));
//    XPC_DUMP(xpc, 3);


    {
    // SecurityManager tests...

    char* t;
    jsval rval;
    JSBool success = JS_TRUE;
    MySecMan* sm = new MySecMan();
    nsTestXPCFoo* foo3 = new nsTestXPCFoo();

    if(!sm || ! foo3)
    {
        success = JS_FALSE;
        printf("failed to create object!\n");
        goto sm_test_done;
    }

    rval = JSVAL_FALSE;
    JS_SetProperty(jscontext, glob, "failed", &rval);
    printf("Individual SecurityManager tests...\n");
    if(!NS_SUCCEEDED(xpc->SetSecurityManagerForJSContext(jscontext, sm,
                                        nsIXPCSecurityManager::HOOK_ALL)))
    {
        success = JS_FALSE;
        printf("SetSecurityManagerForJSContext failed!\n");
        goto sm_test_done;
    }

    sm->SetMode(MySecMan::OK_ALL);
    printf("  getService no veto: ");
    t = "try{Components.classes.nsIXPConnect.getService(); print('passed');}catch(e){failed = true; print('failed');}";
    JS_EvaluateScript(jscontext, glob, t, strlen(t), "builtin", 1, &rval);

    sm->SetMode(MySecMan::VETO_ALL);
    printf("  getService with veto: ");
    t = "try{Components.classes.nsIXPConnect.getService(); failed = true; print('failed');}catch(e){print('passed');}";
    JS_EvaluateScript(jscontext, glob, t, strlen(t), "builtin", 1, &rval);


    sm->SetMode(MySecMan::OK_ALL);
    printf("  createInstance no veto: ");
    t = "try{Components.classes.nsIID.createInstance(Components.interfaces.nsIJSIID); print('passed');}catch(e){failed = true; print('failed');}";
    JS_EvaluateScript(jscontext, glob, t, strlen(t), "builtin", 1, &rval);

    sm->SetMode(MySecMan::VETO_ALL);
    printf("  getService with veto: ");
    t = "try{Components.classes.nsIID.createInstance(Components.interfaces.nsIJSIID); failed = true; print('failed');}catch(e){print('passed');}";
    JS_EvaluateScript(jscontext, glob, t, strlen(t), "builtin", 1, &rval);


    sm->SetMode(MySecMan::OK_ALL);
    printf("  call method no veto: ");
    t = "try{foo.Test2(); print(' : passed');}catch(e){failed = true; print(' : failed');}";
    JS_EvaluateScript(jscontext, glob, t, strlen(t), "builtin", 1, &rval);

    sm->SetMode(MySecMan::VETO_ALL);
    printf("  call method with veto: ");
    t = "try{foo.Test2(); failed = true; print(' : failed');}catch(e){print(' : passed');}";
    JS_EvaluateScript(jscontext, glob, t, strlen(t), "builtin", 1, &rval);


    sm->SetMode(MySecMan::OK_ALL);
    printf("  get attribute no veto: ");
    t = "try{foo.Foo; print(' : passed');}catch(e){failed = true; print(' : failed');}";
    JS_EvaluateScript(jscontext, glob, t, strlen(t), "builtin", 1, &rval);

    sm->SetMode(MySecMan::VETO_ALL);
    printf("  get attribute with veto: ");
    t = "try{foo.Foo; failed = true; print(' : failed');}catch(e){print(' : passed');}";
    JS_EvaluateScript(jscontext, glob, t, strlen(t), "builtin", 1, &rval);


    sm->SetMode(MySecMan::OK_ALL);
    printf("  set attribute no veto: ");
    t = "try{foo.Foo = 0; print(' : passed');}catch(e){failed = true; print(' : failed');}";
    JS_EvaluateScript(jscontext, glob, t, strlen(t), "builtin", 1, &rval);

    sm->SetMode(MySecMan::VETO_ALL);
    printf("  set attribute with veto: ");
    t = "try{foo.Foo = 0; failed = true; print(' : failed');}catch(e){print(' : passed');}";
    JS_EvaluateScript(jscontext, glob, t, strlen(t), "builtin", 1, &rval);

    sm->SetMode(MySecMan::OK_ALL);
    printf("  build wrapper no veto: ");
    if(NS_SUCCEEDED(xpc->WrapNative(jscontext, foo, nsITestXPCFoo2::GetIID(), &wrapper3)))
    {
        printf("passed\n");
        NS_RELEASE(wrapper3);
    }
    else
    {
        success = JS_FALSE;
        printf("failed\n");
    }

    sm->SetMode(MySecMan::VETO_ALL);
    printf("  build wrapper with veto: ");
    if(NS_SUCCEEDED(xpc->WrapNative(jscontext, foo3, nsITestXPCFoo2::GetIID(), &wrapper3)))
    {
        success = JS_FALSE;
        printf("failed\n");
        NS_RELEASE(wrapper3);
    }
    else
    {
        printf("passed\n");
    }

sm_test_done:
    success = success && JS_GetProperty(jscontext, glob, "failed", &rval) && JSVAL_TRUE != rval;
    printf("SecurityManager tests : %s\n", success ? "passed" : "failed");
    NS_RELEASE(foo3);
    }


    {
        // arg formatter test...

        jsval* argv;
        void* mark;

        const char* a_in = "some string";
        const char* b_in = "another meaningless chunck of text";
        char* a_out;
        char* b_out;

        printf("ArgumentFormatter test: ");

        argv = JS_PushArguments(jscontext, &mark, "s %ip s",
                                a_in, &nsITestXPCFoo2::GetIID(), foo, b_in);

        if(argv)
        {
            nsISupports* fooc;
            nsTestXPCFoo* foog;
            if(JS_ConvertArguments(jscontext, 3, argv, "s %ip s",
                                   &a_out, &fooc, &b_out))
            {
                if(fooc)
                {
                    if(NS_SUCCEEDED(fooc->QueryInterface(nsTestXPCFoo::GetIID(),
                                    (void**)&foog)))
                    {
                        if(foog == foo)
                        {
                            if(!strcmp(a_in, a_out) && !strcmp(b_in, b_out))
                                printf("passed\n");
                            else
                                printf(" conversion OK, but surrounding was mangled -- failed!\n");
                        }
                        else
                            printf(" JS to native returned wrong value -- failed!\n");
                        NS_RELEASE(foog);
                    }
                    else
                    {
                        printf(" could not QI value JS to native returned -- failed!\n");
                    }
                    NS_RELEASE(fooc);
                }
                else
                {
                    printf(" JS to native returned NULL -- failed!\n");
                }
            }
            else
            {
                printf(" could not convert from JS to native -- failed!\n");
            }
            JS_PopArguments(jscontext, mark);
        }
        else
        {
            printf(" could not convert from native to JS -- failed!\n");
        }
    }


    {
        // ThreadJSContextStack test

        printf("ThreadJSContextStack tests...\n");

        nsresult rv;
        NS_WITH_SERVICE(nsIJSContextStack, stack, "nsThreadJSContextStack", &rv);

        if(NS_SUCCEEDED(rv))
        {
            PRInt32 count;
            if(NS_SUCCEEDED(stack->GetCount(&count)))
                printf("\tstack->GetCount() : %s\n",
                        count == 0 ? "passed" : "failed!");
            else
                printf("\tstack->GetCount() failed!\n");

            if(NS_FAILED(stack->Push(jscontext)))
                printf("\tstack->Push() failed!\n");
            else
                printf("\tstack->Push() passed\n");

            if(NS_SUCCEEDED(stack->GetCount(&count)))
                printf("\tstack->GetCount() : %s\n",
                        count == 1 ? "passed" : "failed!");
            else
                printf("\tstack->GetCount() failed!\n");

            JSContext* testCX;
            if(NS_FAILED(stack->Peek(&testCX)))
                printf("\tstack->Peek() failed!\n");

            if(jscontext == testCX)
                printf("\tstack->Push/Peek : passed\n");
            else
                printf("\tstack->Push/Peek : failed\n");

            if(NS_FAILED(stack->Pop(&testCX)))
                printf("\tstack->Pop() failed!\n");

            if(jscontext == testCX)
                printf("\tstack->Push/Pop : passed\n");
            else
                printf("\tstack->Push/Pop : failed\n");

            if(NS_SUCCEEDED(stack->GetCount(&count)))
                printf("\tstack->GetCount() : %s\n",
                        count == 0 ? "passed" : "failed!");
            else
                printf("\tstack->GetCount() failed!\n");
        }
        else
            printf("\tfailed to get nsThreadJSContextStack service!\n");

    }


    // cleanup

    NS_RELEASE(foo);

    if(glob)
    {
        JS_DeleteProperty(jscontext, glob, "foo");
        JS_DeleteProperty(jscontext, glob, "echo");
        JS_DeleteProperty(jscontext, glob, "bar");
        JS_DeleteProperty(jscontext, glob, "foo2");
        JS_DeleteProperty(jscontext, glob, "baz");
        JS_DeleteProperty(jscontext, glob, "baz2");
        JS_DeleteProperty(jscontext, glob, "receiver");
        JS_SetGlobalObject(jscontext, JS_NewObject(jscontext, &global_class, NULL, NULL));
    }
    NS_RELEASE(wrapper);

    NS_RELEASE(fool_wrapper);

//    js_DumpGCHeap = stdout;

//    JS_GC(jscontext);
//    printf("-----------------------\n");
//    JS_GC(jscontext);

    // dump to log test...
//    XPC_LOG_ALWAYS((""));
//    XPC_LOG_ALWAYS(("after running JS_GC..."));
//    XPC_LOG_ALWAYS((""));
//    XPC_DUMP(xpc, 3);

#endif

//    xpc->DebugDump(3);
//    printf("-----------------------\n");
    xpc->AbandonJSContext(jscontext);
    JS_DestroyContext(jscontext);
    NS_RELEASE(xpc);
    JS_DestroyRuntime(rt);
    JS_ShutDown();
    return 0;
}

/***************************************************************************/

#include "jsatom.h"
#ifdef DEBUG
int
DumpAtom(JSHashEntry *he, int i, void *arg)
{
    FILE *fp = (FILE *)arg;
    JSAtom *atom = (JSAtom *)he;

    fprintf(fp, "%3d %08x %5lu ",
            i, (uintN)he->keyHash, (unsigned long)atom->number);
    if (ATOM_IS_STRING(atom))
        fprintf(fp, "\"%s\"\n", ATOM_BYTES(atom));
    else if (ATOM_IS_INT(atom))
        fprintf(fp, "%ld\n", (long)ATOM_TO_INT(atom));
    else
        fprintf(fp, "%.16g\n", *ATOM_TO_DOUBLE(atom));
    return HT_ENUMERATE_NEXT;
}

int
DumpSymbol(JSHashEntry *he, int i, void *arg)
{
    FILE *fp = (FILE *)arg;
    JSSymbol *sym = (JSSymbol *)he;

    fprintf(fp, "%3d %08x", i, (uintN)he->keyHash);
    if (JSVAL_IS_INT(sym_id(sym)))
        fprintf(fp, " [%ld]\n", (long)JSVAL_TO_INT(sym_id(sym)));
    else
        fprintf(fp, " \"%s\"\n", ATOM_BYTES(sym_atom(sym)));
    return HT_ENUMERATE_NEXT;
}

/* These are callable from gdb. */
JS_BEGIN_EXTERN_C
void Dsym(JSSymbol *sym) { if (sym) DumpSymbol(&sym->entry, 0, gErrFile); }
void Datom(JSAtom *atom) { if (atom) DumpAtom(&atom->entry, 0, gErrFile); }
//void Dobj(nsISupports* p, int depth) {if(p)XPC_DUMP(p,depth);}
//void Dxpc(int depth) {Dobj(GetXPConnect(), depth);}
JS_END_EXTERN_C
#endif
