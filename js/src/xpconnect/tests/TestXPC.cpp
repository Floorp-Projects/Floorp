/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   IBM Corp.
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* API tests for XPConnect - use xpcshell for JS tests. */

#include <stdio.h>

#include "nsIXPConnect.h"
#include "nsIScriptError.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIJSContextStack.h"
#include "nsIJSRuntimeService.h"
#include "nsMemory.h"
#include "nsIXPCSecurityManager.h"
#include "nsICategoryManager.h"
#include "nsAWritableString.h"

#include "jsapi.h"
#include "jsgc.h"   // for js_ForceGC

#include "xpctest.h"

/***************************************************************************/
// initialization stuff for the xpcom runtime

static void SetupRegistry()
{
    nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, nsnull);
}

/***************************************************************************/
// host support for jsengine

FILE *gOutFile = NULL;
FILE *gErrFile = NULL;

JS_STATIC_DLL_CALLBACK(JSBool)
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

JS_STATIC_DLL_CALLBACK(JSBool)
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

JS_STATIC_DLL_CALLBACK(void)
my_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
    printf(message);
}

/***************************************************************************/
// Foo class for used with some of the tests

class nsTestXPCFoo : public nsITestXPCFoo2
{
    NS_DECL_ISUPPORTS
    NS_DECL_NSITESTXPCFOO

    nsTestXPCFoo();
    virtual ~nsTestXPCFoo();
    char* mFoo;
};

NS_IMETHODIMP nsTestXPCFoo::Test(int p1, int p2, int* retval)
{
//    printf("nsTestXPCFoo::Test called with p1 = %d and p2 = %d\n", p1, p2);
    *retval = p1+p2;
    return NS_OK;
}
NS_IMETHODIMP nsTestXPCFoo::Test2()
{
//    printf("nsTestXPCFoo::Test2 called ");
    return NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::GetFoo(char * *aFoo)
{
//    printf("nsTestXPCFoo::Get called ");
    if(!aFoo)
        return NS_ERROR_NULL_POINTER;
    if(mFoo)
        *aFoo = (char*) nsMemory::Clone(mFoo, strlen(mFoo)+1);
    else
        *aFoo = NULL;
    return NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::SetFoo(const char * aFoo)
{
//    printf("nsTestXPCFoo::Set called ");
    if(mFoo)
    {
        nsMemory::Free(mFoo);
        mFoo = NULL;
    }
    if(aFoo)
        mFoo = (char*) nsMemory::Clone(aFoo, strlen(aFoo)+1);
    return NS_OK;
}

NS_IMPL_ISUPPORTS2(nsTestXPCFoo, nsITestXPCFoo, nsITestXPCFoo2)

nsTestXPCFoo::nsTestXPCFoo()
    : mFoo(NULL)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

nsTestXPCFoo::~nsTestXPCFoo()
{
    if(mFoo)
        nsMemory::Free(mFoo);
}

/***************************************************************************/
// test for nsIXPCSecurityManager

class MySecMan : public nsIXPCSecurityManager
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIXPCSECURITYMANAGER

  enum Mode { OK_ALL    ,
              VETO_ALL
            };

  void SetMode(Mode mode) {mMode = mode;}

  MySecMan();

private:
  Mode mMode;
};

NS_IMPL_ISUPPORTS1(MySecMan, nsIXPCSecurityManager)

MySecMan::MySecMan()
    : mMode(OK_ALL)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

NS_IMETHODIMP
MySecMan::CanCreateWrapper(JSContext * aJSContext, const nsIID & aIID, nsISupports *aObj, nsIClassInfo *aClassInfo, void * *aPolicy)
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

/* void CanAccess (in PRUint32 aAction, in nsIXPCNativeCallContext aCallContext, in JSContextPtr aJSContext, in JSObjectPtr aJSObject, in nsISupports aObj, in nsIClassInfo aClassInfo, in JSVal aName, inout voidPtr aPolicy); */
NS_IMETHODIMP 
MySecMan::CanAccess(PRUint32 aAction, nsIXPCNativeCallContext *aCallContext, JSContext * aJSContext, JSObject * aJSObject, nsISupports *aObj, nsIClassInfo *aClassInfo, jsval aName, void * *aPolicy)
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

/**********************************************/

static void
TestSecurityManager(JSContext* jscontext, JSObject* glob, nsIXPConnect* xpc)
{
    char* t;
    jsval rval;
    JSBool success = JS_TRUE;
    MySecMan* sm = new MySecMan();
    nsTestXPCFoo* foo = new nsTestXPCFoo();

    if(!sm || ! foo)
    {
        success = JS_FALSE;
        printf("FAILED to create object!\n");
        goto sm_test_done;
    }

    rval = JSVAL_FALSE;
    JS_SetProperty(jscontext, glob, "failed", &rval);
    printf("Individual SecurityManager tests...\n");
    if(!NS_SUCCEEDED(xpc->SetSecurityManagerForJSContext(jscontext, sm,
                                        nsIXPCSecurityManager::HOOK_ALL)))
    {
        success = JS_FALSE;
        printf("SetSecurityManagerForJSContext FAILED!\n");
        goto sm_test_done;
    }

    printf("  build wrapper with veto: TEST NOT RUN\n");
/*
    // This test is broken because xpconnect now detects that this is a
    // call from native code and lets it succeed without calling the security manager

    sm->SetMode(MySecMan::VETO_ALL);
    printf("  build wrapper with veto: ");
    if(NS_SUCCEEDED(xpc->WrapNative(jscontext, glob, foo, NS_GET_IID(nsITestXPCFoo2), &wrapper)))
    {
        success = JS_FALSE;
        printf("FAILED\n");
        NS_RELEASE(wrapper);
    }
    else
    {
        printf("passed\n");
    }
*/
    sm->SetMode(MySecMan::OK_ALL);
    printf("  build wrapper no veto: ");
    nsIXPConnectJSObjectHolder* holder;
    if(NS_SUCCEEDED(xpc->WrapNative(jscontext, glob, foo, 
                    NS_GET_IID(nsITestXPCFoo2), &holder)))
    {
        printf("passed\n");
        JSObject* obj;
        if(NS_SUCCEEDED(holder->GetJSObject(&obj)))
        {
            rval = OBJECT_TO_JSVAL(obj);
            JS_SetProperty(jscontext, glob, "foo", &rval);
        }
            
        NS_RELEASE(holder);
    }
    else
    {
        success = JS_FALSE;
        printf("FAILED\n");
    }

    sm->SetMode(MySecMan::OK_ALL);
    printf("  getService no veto: ");
    t = "try{Components.classes['@mozilla.org/js/xpc/XPConnect;1'].getService(); print('passed');}catch(e){failed = true; print('FAILED');}";
    JS_EvaluateScript(jscontext, glob, t, strlen(t), "builtin", 1, &rval);

    sm->SetMode(MySecMan::VETO_ALL);
    printf("  getService with veto: ");
    t = "try{Components.classes['@mozilla.org/js/xpc/XPConnect;1'].getService(); failed = true; print('FAILED');}catch(e){print('passed');}";
    JS_EvaluateScript(jscontext, glob, t, strlen(t), "builtin", 1, &rval);


    sm->SetMode(MySecMan::OK_ALL);
    printf("  createInstance no veto: ");
    t = "try{Components.classes['@mozilla.org/js/xpc/ID;1'].createInstance(Components.interfaces.nsIJSID); print('passed');}catch(e){failed = true; print('FAILED');}";
    JS_EvaluateScript(jscontext, glob, t, strlen(t), "builtin", 1, &rval);

    sm->SetMode(MySecMan::VETO_ALL);
    printf("  getService with veto: ");
    t = "try{Components.classes['@mozilla.org/js/xpc/ID;1'].createInstance(Components.interfaces.nsIJSID); failed = true; print('FAILED');}catch(e){print('passed');}";
    JS_EvaluateScript(jscontext, glob, t, strlen(t), "builtin", 1, &rval);


    sm->SetMode(MySecMan::OK_ALL);
    printf("  call method no veto: ");
    t = "try{foo.Test2(); print(' : passed');}catch(e){failed = true; print(' : FAILED');}";
    JS_EvaluateScript(jscontext, glob, t, strlen(t), "builtin", 1, &rval);

    sm->SetMode(MySecMan::VETO_ALL);
    printf("  call method with veto: ");
    t = "try{foo.Test2(); failed = true; print(' : FAILED');}catch(e){print(' : passed');}";
    JS_EvaluateScript(jscontext, glob, t, strlen(t), "builtin", 1, &rval);


    sm->SetMode(MySecMan::OK_ALL);
    printf("  get attribute no veto: ");
    t = "try{foo.Foo; print(' : passed');}catch(e){failed = true; print(' : FAILED');}";
    JS_EvaluateScript(jscontext, glob, t, strlen(t), "builtin", 1, &rval);

    sm->SetMode(MySecMan::VETO_ALL);
    printf("  get attribute with veto: ");
    t = "try{foo.Foo; failed = true; print(' : FAILED');}catch(e){print(' : passed');}";
    JS_EvaluateScript(jscontext, glob, t, strlen(t), "builtin", 1, &rval);


    sm->SetMode(MySecMan::OK_ALL);
    printf("  set attribute no veto: ");
    t = "try{foo.Foo = 0; print(' : passed');}catch(e){failed = true; print(' : FAILED');}";
    JS_EvaluateScript(jscontext, glob, t, strlen(t), "builtin", 1, &rval);

    sm->SetMode(MySecMan::VETO_ALL);
    printf("  set attribute with veto: ");
    t = "try{foo.Foo = 0; failed = true; print(' : FAILED');}catch(e){print(' : passed');}";
    JS_EvaluateScript(jscontext, glob, t, strlen(t), "builtin", 1, &rval);

sm_test_done:
    success = success && JS_GetProperty(jscontext, glob, "failed", &rval) && JSVAL_TRUE != rval;
    printf("SecurityManager tests : %s\n", success ? "passed" : "FAILED");
    NS_IF_RELEASE(foo);
    xpc->SetSecurityManagerForJSContext(jscontext, nsnull, 0);
}


/***************************************************************************/
// arg formatter test...

static void
TestArgFormatter(JSContext* jscontext, JSObject* glob, nsIXPConnect* xpc)
{
    jsval* argv;
    void* mark;

    nsTestXPCFoo* foo = new nsTestXPCFoo();
    const char* a_in = "some string";
    const char* b_in = "another meaningless chunck of text";
    char* a_out;
    char* b_out;

    printf("ArgumentFormatter test: ");

    argv = JS_PushArguments(jscontext, &mark, "s %ip s",
                            a_in, &NS_GET_IID(nsITestXPCFoo2), foo, b_in);

    if(argv)
    {
        nsISupports* fooc;
        nsTestXPCFoo* foog;
        if(JS_ConvertArguments(jscontext, 3, argv, "s %ip s",
                               &a_out, &fooc, &b_out))
        {
            if(fooc)
            {
                if(NS_SUCCEEDED(fooc->QueryInterface(NS_GET_IID(nsTestXPCFoo),
                                (void**)&foog)))
                {
                    if(foog == foo)
                    {
                        if(!strcmp(a_in, a_out) && !strcmp(b_in, b_out))
                            printf("passed\n");
                        else
                            printf(" conversion OK, but surrounding was mangled -- FAILED!\n");
                    }
                    else
                        printf(" JS to native returned wrong value -- FAILED!\n");
                    NS_RELEASE(foog);
                }
                else
                {
                    printf(" could not QI value JS to native returned -- FAILED!\n");
                }
                NS_RELEASE(fooc);
            }
            else
            {
                printf(" JS to native returned NULL -- FAILED!\n");
            }
        }
        else
        {
            printf(" could not convert from JS to native -- FAILED!\n");
        }
        JS_PopArguments(jscontext, mark);
    }
    else
    {
        printf(" could not convert from native to JS -- FAILED!\n");
    }
    NS_IF_RELEASE(foo);
}

/***************************************************************************/
// ThreadJSContextStack test

static void
TestThreadJSContextStack(JSContext* jscontext)
{

    printf("ThreadJSContextStack tests...\n");

    nsresult rv;
    nsCOMPtr<nsIJSContextStack> stack = 
             do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);

    if(NS_SUCCEEDED(rv))
    {
        PRInt32 count;
        PRInt32 base_count;

        if(NS_SUCCEEDED(stack->GetCount(&base_count)))
            printf("\tstack->GetCount() : passed\n");
        else
            printf("\tstack->GetCount() FAILED!\n");

        if(NS_FAILED(stack->Push(jscontext)))
            printf("\tstack->Push() FAILED!\n");
        else
            printf("\tstack->Push() passed\n");

        if(NS_SUCCEEDED(stack->GetCount(&count)))
            printf("\tstack->GetCount() : %s\n",
                    count == base_count+1 ? "passed" : "FAILED!");
        else
            printf("\tstack->GetCount() FAILED!\n");

        JSContext* testCX;
        if(NS_FAILED(stack->Peek(&testCX)))
            printf("\tstack->Peek() FAILED!\n");

        if(jscontext == testCX)
            printf("\tstack->Push/Peek : passed\n");
        else
            printf("\tstack->Push/Peek : FAILED\n");

        if(NS_FAILED(stack->Pop(&testCX)))
            printf("\tstack->Pop() FAILED!\n");

        if(jscontext == testCX)
            printf("\tstack->Push/Pop : passed\n");
        else
            printf("\tstack->Push/Pop : FAILED\n");

        if(NS_SUCCEEDED(stack->GetCount(&count)))
            printf("\tstack->GetCount() : %s\n",
                    count == base_count ? "passed" : "FAILED!");
        else
            printf("\tstack->GetCount() FAILED!\n");
    }
    else
        printf("\tFAILED to get nsThreadJSContextStack service!\n");
}

/***************************************************************************/

static void ShowXPCException()
{
    nsresult rv;
    nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
    if(NS_SUCCEEDED(rv) && xpc)
    {
        nsCOMPtr<nsIException> e;
        xpc->GetPendingException(getter_AddRefs(e));
        if(e)
        {
            char* str;
            rv = e->ToString(&str);
            if(NS_SUCCEEDED(rv) && str)
            {
                printf(str);
                printf("\n");
                nsMemory::Free(str);

                nsresult res;
                e->GetResult(&res);
                if(res == NS_ERROR_XPC_JAVASCRIPT_ERROR_WITH_DETAILS)
                {
                    nsCOMPtr<nsISupports> data;
                    e->GetData(getter_AddRefs(data));
                    if(data)
                    {
                        nsCOMPtr<nsIScriptError> report = do_QueryInterface(data);
                        if(report)
                        {
                            char* str2;
                            rv = report->ToString(&str2);
                            if(NS_SUCCEEDED(rv) && str2)
                            {
                                printf(str2);
                                printf("\n");
                                nsMemory::Free(str2);
                            }                            
                        }                            
                    }                            
                    else        
                        printf("can't get data for pending XPC exception\n");    
                }
            }
            else        
                printf("can't get string for pending XPC exception\n");    
        }
        else        
            printf("no pending XPC exception\n");    
    }
    else
        printf("can't get xpconnect\n");    
}

static void TestCategoryManmager()
{
    printf("\n");    

    nsresult rv;
    nsCOMPtr<nsICategoryManager> catman = 
             do_GetService("@mozilla.org/categorymanager;1", &rv);
    if(NS_SUCCEEDED(rv) && catman)
    {
        printf("got category manager\n");    
 
        nsCOMPtr<nsISimpleEnumerator> e;
        rv = catman->EnumerateCategory("foo", getter_AddRefs(e));
        if(NS_SUCCEEDED(rv) && e)
        {
            printf("got enumerator\n");

            nsCOMPtr<nsISupports> el;
            rv = e->GetNext(getter_AddRefs(el));
            if(NS_SUCCEEDED(rv) && el)
            {
                printf("e.GetNext() succeeded\n");
            }
            else
            {
                printf("e.GetNext() FAILED with result %0x\n", (int)rv);        
                ShowXPCException();
            }
                        
        }
        else
            printf("get of enumerator FAILED with result %0x\n", (int)rv);        
    }
    else
        printf("!!! can't get category manager\n");    


    printf("\n");    
}


/***************************************************************************/
/***************************************************************************/
// our main...

#define DIE(_msg) \
    PR_BEGIN_MACRO  \
        printf(_msg); \
        printf("\n"); \
        return 1; \
    PR_END_MACRO

int main()
{
    JSRuntime *rt;
    JSContext *jscontext;
    JSObject *glob;
    nsresult rv;

    gErrFile = stderr;
    gOutFile = stdout;

    SetupRegistry();

    // get the JSRuntime from the runtime svc, if possible
    nsCOMPtr<nsIJSRuntimeService> rtsvc = 
             do_GetService("@mozilla.org/js/xpc/RuntimeService;1", &rv);
    if(NS_FAILED(rv) || NS_FAILED(rtsvc->GetRuntime(&rt)) || !rt)
        DIE("FAILED to get a JSRuntime");

    jscontext = JS_NewContext(rt, 8192);
    if(!jscontext) 
        DIE("FAILED to create a JSContext");

    JS_SetErrorReporter(jscontext, my_ErrorReporter);

    nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
    if(!xpc) 
        DIE("FAILED to get xpconnect service\n");

    nsCOMPtr<nsIJSContextStack> cxstack = 
             do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
    if(NS_FAILED(rv)) 
        DIE("FAILED to get the nsThreadJSContextStack service!\n");

    if(NS_FAILED(cxstack->Push(jscontext)))
        DIE("FAILED to get push the current jscontext on the nsThreadJSContextStack service!\n");

    // XXX I'd like to replace this with code that uses a wrapped xpcom object
    // as the global object. The old TextXPC did this. The support for this 
    // is not working now in the new xpconnect code.

    glob = JS_NewObject(jscontext, &global_class, NULL, NULL);
    if (!glob)
        DIE("FAILED to create global object");
    if (!JS_InitStandardClasses(jscontext, glob))
        DIE("FAILED to init standard classes");
    if (!JS_DefineFunctions(jscontext, glob, glob_functions))
        DIE("FAILED to define global functions");
    if (NS_FAILED(xpc->InitClasses(jscontext, glob)))
        DIE("FAILED to init xpconnect classes");

    /**********************************************/
    // run the tests...

    TestCategoryManmager();
    TestSecurityManager(jscontext, glob, xpc);
    TestArgFormatter(jscontext, glob, xpc);
    TestThreadJSContextStack(jscontext);

    /**********************************************/
    JS_ClearScope(jscontext, glob);
    js_ForceGC(jscontext);
    js_ForceGC(jscontext);
    JS_DestroyContext(jscontext);
    xpc->SyncJSContexts();
    xpc->DebugDump(4);

    cxstack = nsnull;   // release service held by nsCOMPtr
    xpc     = nsnull;   // release service held by nsCOMPtr
    rtsvc   = nsnull;   // release service held by nsCOMPtr

    rv = NS_ShutdownXPCOM( NULL );
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM FAILED");

    return 0;
}

