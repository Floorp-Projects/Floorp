/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corp.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/* API tests for XPConnect - use xpcshell for JS tests. */

#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>

#include "jsapi.h"
#include "jscntxt.h"

#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsIXPConnect.h"
#include "nsIScriptError.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIJSContextStack.h"
#include "nsIJSRuntimeService.h"
#include "nsMemory.h"
#include "nsIXPCSecurityManager.h"
#include "nsICategoryManager.h"
#include "nsIVariant.h"
#include "nsStringAPI.h"
#include "nsEmbedString.h"

#include "xpctest.h"

/***************************************************************************/
// host support for jsengine

FILE *gOutFile = NULL;
FILE *gErrFile = NULL;

static JSBool
Print(JSContext *cx, uintN argc, jsval *vp)
{
    uintN i, n;
    JSString *str;

    jsval *argv = JS_ARGV(cx, vp);
    for (i = n = 0; i < argc; i++) {
        str = JS_ValueToString(cx, argv[i]);
        if (!str)
            return JS_FALSE;
        fprintf(gOutFile, "%s%s", i ? " " : "", JS_GetStringBytes(str));
    }
    n++;
    if (n)
        fputc('\n', gOutFile);
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
Load(JSContext *cx, uintN argc, jsval *vp)
{
    uintN i;
    JSString *str;
    const char *filename;
    JSScript *script;
    JSBool ok;
    jsval result;

    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;

    jsval *argv = JS_ARGV(cx, vp);
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
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSFunctionSpec glob_functions[] = {
    {"print",           Print,          0,0},
    {"load",            Load,           1,0},
    {nsnull,nsnull,0,0}
};

static JSClass global_class = {
    "global", 0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   nsnull
};

static void
my_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
    fputs(message, stdout);
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
            NS_ERROR("bad case");
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
            NS_ERROR("bad case");
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
            NS_ERROR("bad case");
            return NS_OK;
    }
}

/* void CanAccess (in PRUint32 aAction, in nsIXPCNativeCallContext aCallContext, in JSContextPtr aJSContext, in JSObjectPtr aJSObject, in nsISupports aObj, in nsIClassInfo aClassInfo, in jsid aName, inout voidPtr aPolicy); */
NS_IMETHODIMP 
MySecMan::CanAccess(PRUint32 aAction, nsAXPCNativeCallContext *aCallContext, JSContext * aJSContext, JSObject * aJSObject, nsISupports *aObj, nsIClassInfo *aClassInfo, jsid aName, void * *aPolicy)
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
            NS_ERROR("bad case");
            return NS_OK;
    }
}

/**********************************************/
static void
EvaluateScript(JSContext* jscontext, JSObject* glob, MySecMan* sm, MySecMan::Mode mode, const char* msg, const char* t, jsval &rval)
{
    sm->SetMode(mode);
    fputs(msg, stdout);
    JSAutoRequest ar(jscontext);
    JS_EvaluateScript(jscontext, glob, t, strlen(t), "builtin", 1, &rval);
}

static void
TestSecurityManager(JSContext* jscontext, JSObject* glob, nsIXPConnect* xpc)
{
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
    {
        JSAutoRequest ar(jscontext);
        JS_SetProperty(jscontext, glob, "failed", &rval);
    }
    printf("Individual SecurityManager tests...\n");
    if(NS_FAILED(xpc->SetSecurityManagerForJSContext(jscontext, sm,
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
            JSAutoRequest ar(jscontext);
            JS_SetProperty(jscontext, glob, "foo", &rval);
        }
            
        NS_RELEASE(holder);
    }
    else
    {
        success = JS_FALSE;
        printf("FAILED\n");
    }

    EvaluateScript(jscontext, glob, sm, MySecMan::OK_ALL,
                   "  getService no veto: ",
                   "try{Components.classes['@mozilla.org/js/xpc/XPConnect;1'].getService(); print('passed');}catch(e){failed = true; print('FAILED');}",
                   rval);

    EvaluateScript(jscontext, glob, sm, MySecMan::VETO_ALL,
                   "  getService with veto: ",
                   "try{Components.classes['@mozilla.org/js/xpc/XPConnect;1'].getService(); failed = true; print('FAILED');}catch(e){print('passed');}",
                   rval);


    EvaluateScript(jscontext, glob, sm, MySecMan::OK_ALL,
                   "  createInstance no veto: ",
                   "try{Components.classes['@mozilla.org/js/xpc/ID;1'].createInstance(Components.interfaces.nsIJSID); print('passed');}catch(e){failed = true; print('FAILED');}",
                   rval);

    EvaluateScript(jscontext, glob, sm, MySecMan::VETO_ALL,
                   "  getService with veto: ",
                   "try{Components.classes['@mozilla.org/js/xpc/ID;1'].createInstance(Components.interfaces.nsIJSID); failed = true; print('FAILED');}catch(e){print('passed');}",
                   rval);


    EvaluateScript(jscontext, glob, sm, MySecMan::OK_ALL,
                   "  call method no veto: ",
                   "try{foo.Test2(); print(' : passed');}catch(e){failed = true; print(' : FAILED');}",
                   rval);

    EvaluateScript(jscontext, glob, sm, MySecMan::VETO_ALL,
                   "  call method with veto: ",
                   "try{foo.Test2(); failed = true; print(' : FAILED');}catch(e){print(' : passed');}",
                   rval);


    EvaluateScript(jscontext, glob, sm, MySecMan::OK_ALL,
                   "  get attribute no veto: ",
                   "try{foo.Foo; print(' : passed');}catch(e){failed = true; print(' : FAILED');}",
                   rval);

    EvaluateScript(jscontext, glob, sm, MySecMan::VETO_ALL,
                   "  get attribute with veto: ",
                   "try{foo.Foo; failed = true; print(' : FAILED');}catch(e){print(' : passed');}",
                   rval);


    EvaluateScript(jscontext, glob, sm, MySecMan::OK_ALL,
                   "  set attribute no veto: ",
                   "try{foo.Foo = 0; print(' : passed');}catch(e){failed = true; print(' : FAILED');}",
                   rval);

    EvaluateScript(jscontext, glob, sm, MySecMan::VETO_ALL,
                   "  set attribute with veto: ",
                   "try{foo.Foo = 0; failed = true; print(' : FAILED');}catch(e){print(' : passed');}",
                   rval);

sm_test_done:
    {
        JSAutoRequest ar(jscontext);
        success = success && JS_GetProperty(jscontext, glob, "failed", &rval) && JSVAL_TRUE != rval;
    }
    printf("SecurityManager tests : %s\n", success ? "passed" : "FAILED");
    NS_IF_RELEASE(foo);
    xpc->SetSecurityManagerForJSContext(jscontext, nsnull, 0);
}


/***************************************************************************/
// arg formatter test...

// A bit of history: JS_PushArguments/JS_PushArgumentsVA used to be part of the
// JS public (friend, really) API using js_AllocStack to obtain the rooted
// output array. js_AllocStack was removed and, to preserve the ability to test
// JS_ConvertArguments, these functions were moved here and hacked down to
// size.

#ifdef HAVE_VA_LIST_AS_ARRAY
#define JS_ADDRESSOF_VA_LIST(ap) ((va_list *)(ap))
#else
#define JS_ADDRESSOF_VA_LIST(ap) (&(ap))
#endif

static JSBool
TryArgumentFormatter(JSContext *cx, const char **formatp, JSBool fromJS,
                     jsval **vpp, va_list *app)
{
    const char *format;
    JSArgumentFormatMap *map;

    format = *formatp;
    for (map = cx->argumentFormatMap; map; map = map->next) {
        if (!strncmp(format, map->format, map->length)) {
            *formatp = format + map->length;
            return map->formatter(cx, format, fromJS, vpp, app);
        }
    }
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_CHAR, format);
    return JS_FALSE;
}

static bool
PushArgumentsVA(JSContext *cx, uintN argc, jsval *argv, const char *format, va_list ap)
{
    char c;
    JSString *str;

    jsval *sp = argv;

    while ((c = *format++) != '\0') {
        if (isspace(c) || c == '*')
            continue;
        switch (c) {
          case 's':
            str = JS_NewStringCopyZ(cx, va_arg(ap, char *));
            if (!str)
                return false;
            *sp = STRING_TO_JSVAL(str);
            break;
          default:
            format--;
            if (!TryArgumentFormatter(cx, &format, JS_FALSE, &sp, JS_ADDRESSOF_VA_LIST(ap)))
                return false;
            /* NB: the formatter already updated sp, so we continue here. */
            continue;
        }
        sp++;
    }

    return true;
}

static bool
PushArguments(JSContext *cx, uintN argc, jsval *argv, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    bool ret = PushArgumentsVA(cx, argc, argv, format, ap);
    va_end(ap);
    return ret;
}

#define TAF_CHECK(cond, msg) \
    if (!cond) {       \
        printf(msg);   \
        ok = JS_FALSE; \
        break;         \
    }

static void
TestArgFormatter(JSContext* jscontext, JSObject* glob, nsIXPConnect* xpc)
{
    JSBool ok = JS_TRUE;

    const char*                  a_in = "some string";
    nsCOMPtr<nsITestXPCFoo>      b_in = new nsTestXPCFoo();
    nsCOMPtr<nsIWritableVariant> c_in = do_CreateInstance("@mozilla.org/variant;1"); 
    static NS_NAMED_LITERAL_STRING(d_in, "foo bar");
    const char*                  e_in = "another meaningless chunck of text";
    

    JSString*               a_out;
    nsCOMPtr<nsISupports>   b_out;
    nsCOMPtr<nsIVariant>    c_out;
    nsAutoString            d_out;
    JSString*               e_out;

    nsCOMPtr<nsITestXPCFoo> specified;
    PRInt32                 val;

    printf("ArgumentFormatter test: ");

    if(!b_in || !c_in || NS_FAILED(c_in->SetAsInt32(5)))
    {
        printf(" failed to construct test objects -- FAILED!\n");
        return;
    }

    do {
        JSAutoRequest ar(jscontext);

        // Prepare an array of arguments for JS_ConvertArguments
        jsval argv[5];
        js::AutoArrayRooter tvr(jscontext, JS_ARRAY_LENGTH(argv), argv);

        if (!PushArguments(jscontext, 5, argv,
                           "s %ip %iv %is s",
                           a_in,
                           &NS_GET_IID(nsITestXPCFoo2), b_in.get(),
                           c_in.get(),
                           static_cast<const nsAString*>(&d_in),
                           e_in))
        {
            printf(" could not convert from native to JS -- FAILED!\n");
            return;
        }

        ok = JS_ConvertArguments(jscontext, 5, argv, "S %ip %iv %is S",
                                &a_out, 
                                static_cast<nsISupports**>(getter_AddRefs(b_out)), 
                                static_cast<nsIVariant**>(getter_AddRefs(c_out)),
                                static_cast<nsAString*>(&d_out), 
                                 &e_out);
        TAF_CHECK(ok, " could not convert from JS to native -- FAILED!\n");
        TAF_CHECK(b_out, " JS to native for %%ip returned NULL -- FAILED!\n");
    
        specified = do_QueryInterface(b_out);
        TAF_CHECK(specified, " could not QI value JS to native returned -- FAILED!\n");
        ok = specified.get() == b_in.get();
        TAF_CHECK(ok, " JS to native returned wrong value -- FAILED!\n");
        TAF_CHECK(c_out, " JS to native for %%iv returned NULL -- FAILED!\n");
        TAF_CHECK(NS_SUCCEEDED(c_out->GetAsInt32(&val)) && val == 5, " JS to native for %%iv holds wrong value -- FAILED!\n");
        TAF_CHECK(d_in.Equals(d_out), " JS to native for %%is returned the wrong value -- FAILED!\n");
    } while (0);
    if (!ok)
        return;

    if(JS_MatchStringAndAscii(a_out, a_in) && JS_MatchStringAndAscii(e_out, e_in))
        printf("passed\n");
    else
        printf(" conversion OK, but surrounding was mangled -- FAILED!\n");
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
                printf("%s\n", str);
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
                            nsCAutoString str2;
                            rv = report->ToString(str2);
                            if(NS_SUCCEEDED(rv))
                            {
                                printf("%s\n", str2.get());
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
    {
        nsCOMPtr<nsIServiceManager> servMan;
        NS_InitXPCOM2(getter_AddRefs(servMan), nsnull, nsnull);
    
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
            DIE("FAILED to push the current jscontext on the nsThreadJSContextStack service!\n");

        // XXX I'd like to replace this with code that uses a wrapped xpcom object
        // as the global object. The old TextXPC did this. The support for this
        // is not working now in the new xpconnect code.

        {
            JSAutoRequest ar(jscontext);
            glob = JS_NewCompartmentAndGlobalObject(jscontext, &global_class, NULL);
            if (!glob)
                DIE("FAILED to create global object");

            JSAutoEnterCompartment ac;
            if (!ac.enter(jscontext, glob))
                DIE("FAILED to enter compartment");

            if (!JS_InitStandardClasses(jscontext, glob))
                DIE("FAILED to init standard classes");
            if (!JS_DefineFunctions(jscontext, glob, glob_functions))
                DIE("FAILED to define global functions");
            if (NS_FAILED(xpc->InitClasses(jscontext, glob)))
                DIE("FAILED to init xpconnect classes");
        }

        /**********************************************/
        // run the tests...

        TestCategoryManmager();
        TestSecurityManager(jscontext, glob, xpc);
        TestArgFormatter(jscontext, glob, xpc);
        TestThreadJSContextStack(jscontext);

        /**********************************************/

        if(NS_FAILED(cxstack->Pop(nsnull)))
            DIE("FAILED to pop the current jscontext from the nsThreadJSContextStack service!\n");

        {
            JSAutoRequest ar(jscontext);
            JS_ClearScope(jscontext, glob);
            JS_GC(jscontext);
            JS_GC(jscontext);
        }
        JS_DestroyContext(jscontext);
        xpc->DebugDump(4);

        cxstack = nsnull;   // release service held by nsCOMPtr
        xpc     = nsnull;   // release service held by nsCOMPtr
        rtsvc   = nsnull;   // release service held by nsCOMPtr
    }
    rv = NS_ShutdownXPCOM( NULL );
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM FAILED");

    return 0;
}

