
#include "nsIXPConnect.h"
#include "nsIXPCScriptable.h"
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIXPCScriptable.h"
#include "jsapi.h"
#include <stdio.h>


// XXX this should not be necessary, but the nsIAllocator service is not being 
// started right now.
#include "nsAllocator.h"
#include "nsRepository.h"

/***************************************************************************/
/***************************************************************************/
// copying in the contents of nsAllocator.cpp as a test...

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIAllocatorIID, NS_IALLOCATOR_IID);

nsAllocator::nsAllocator(nsISupports* outer)
{
    NS_INIT_AGGREGATED(outer);
}

nsAllocator::~nsAllocator(void)
{
}

NS_IMPL_AGGREGATED(nsAllocator);

NS_METHOD
nsAllocator::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
    if (NULL == aInstancePtr) {                                            
        return NS_ERROR_NULL_POINTER;                                        
    }                                                                      
    if (aIID.Equals(kIAllocatorIID) || 
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) this; 
        AddRef(); 
        return NS_OK; 
    } 
    return NS_NOINTERFACE;
}

NS_METHOD
nsAllocator::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    if (outer && !aIID.Equals(kISupportsIID))
        return NS_NOINTERFACE;   // XXX right error?
    nsAllocator* mm = new nsAllocator(outer);
    if (mm == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    mm->AddRef();
    if (aIID.Equals(kISupportsIID))
        *aInstancePtr = mm->GetInner();
    else
        *aInstancePtr = mm;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_METHOD_(void*)
nsAllocator::Alloc(PRUint32 size)
{
    return PR_Malloc(size);
}

NS_METHOD_(void*)
nsAllocator::Realloc(void* ptr, PRUint32 size)
{
    return PR_Realloc(ptr, size);
}

NS_METHOD
nsAllocator::Free(void* ptr)
{
    PR_Free(ptr);
    return NS_OK;
}

NS_METHOD
nsAllocator::HeapMinimize(void)
{
#ifdef XP_MAC
    // This used to live in the memory allocators no Mac, but does no more
    // Needs to be hooked up in the new world.
//    CallCacheFlushers(0x7fffffff);
#endif
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

nsAllocatorFactory::nsAllocatorFactory(void)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

nsAllocatorFactory::~nsAllocatorFactory(void)
{
}

static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS(nsAllocatorFactory, kIFactoryIID);

NS_METHOD
nsAllocatorFactory::CreateInstance(nsISupports *aOuter,
                                   REFNSIID aIID,
                                   void **aResult)
{
    return nsAllocator::Create(aOuter, aIID, aResult);
}

NS_METHOD
nsAllocatorFactory::LockFactory(PRBool aLock)
{
    return NS_OK;       // XXX what?
}

////////////////////////////////////////////////////////////////////////////////
/***************************************************************************/
/***************************************************************************/

static void RegAllocator()
{
    static NS_DEFINE_IID(kAllocatorCID, NS_ALLOCATOR_CID);
    nsRepository::RegisterFactory(kAllocatorCID,
                                  (nsIFactory*)new nsAllocatorFactory(),
                                  PR_FALSE);
}    


/***************************************************************************/
class MyScriptable : public nsIXPCScriptable
{
    NS_DECL_ISUPPORTS;
    XPC_DECLARE_IXPCSCRIPTABLE;
};

NS_IMPL_ISUPPORTS(MyScriptable,NS_IXPCSCRIPTABLE_IID);

// XPC_IMPLEMENT_FORWARD_IXPCSCRIPTABLE(MyScriptable);
    XPC_IMPLEMENT_FORWARD_CREATE(MyScriptable);
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
    NS_DECL_ISUPPORTS;
    NS_IMETHOD Test(int p1, int p2, int* retval);
    NS_IMETHOD Test2();
    nsTestXPCFoo();
};

NS_IMETHODIMP nsTestXPCFoo::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  *aInstancePtr = NULL;

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  static NS_DEFINE_IID(kClassIID, NS_ITESTXPC_FOO_IID);
  static NS_DEFINE_IID(kClass2IID, NS_ITESTXPC_FOO2_IID);
  static NS_DEFINE_IID(kScriptableIID, NS_IXPCSCRIPTABLE_IID);

  if (aIID.Equals(kClassIID) || 
      aIID.Equals(kClass2IID) ||
      aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kScriptableIID)) {
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
    printf("nsTestXPCFoo::Test2 called\n");
    return NS_OK;
}


NS_IMPL_ADDREF(nsTestXPCFoo)
NS_IMPL_RELEASE(nsTestXPCFoo)

nsTestXPCFoo::nsTestXPCFoo()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

static NS_DEFINE_IID(kIFooIID, NS_ITESTXPC_FOO_IID);
static NS_DEFINE_IID(kIFoo2IID, NS_ITESTXPC_FOO2_IID);

static NS_DEFINE_IID(kWrappedJSMethodsIID, NS_IXPCONNECT_WRAPPED_JS_METHODS_IID);

/***************************************************************************/
FILE *gOutFile = NULL;

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

static JSFunctionSpec glob_functions[] = {
    {"print",           Print,          0},
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

int main()
{
    JSRuntime *rt;
    JSContext *cx;
    JSObject *glob;

    gOutFile = stdout;

    RegAllocator();

    rt = JS_NewRuntime(8L * 1024L * 1024L);
    if (!rt)
        return 1;
    cx = JS_NewContext(rt, 8192);
    if (!cx)
        return 1;

    JS_SetErrorReporter(cx, my_ErrorReporter);

    glob = JS_NewObject(cx, &global_class, NULL, NULL);
    if (!glob)
        return 1;

    if (!JS_InitStandardClasses(cx, glob))
        return 1;
    if (!JS_DefineFunctions(cx, glob, glob_functions))
        return 1;

    nsIXPConnect* xpc = XPC_GetXPConnect();
    xpc->InitJSContext(cx, glob);

    nsTestXPCFoo* foo = new nsTestXPCFoo();

//    nsXPCVarient v[2];
//    v[0].type = nsXPCType::T_I32; v[0].val.i32 = 1;
//    v[1].type = nsXPCType::T_I32; v[1].val.i32 = 2;

//    XPC_TestInvoke(foo, 3, 2, v);
//    XPC_TestInvoke(foo, 4, 0, NULL);

    nsIXPConnectWrappedNative* wrapper;
    nsIXPConnectWrappedNative* wrapper2;
    if(NS_SUCCEEDED(xpc->WrapNative(cx, foo, kIFooIID, &wrapper)))
    {
        if(NS_SUCCEEDED(xpc->WrapNative(cx, foo, kIFoo2IID, &wrapper2)))
        {
            JSObject* js_obj;
            nsISupports* com_obj;
            jsval rval;

            wrapper2->GetJSObject(&js_obj);
            wrapper2->GetNative(&com_obj);

            jsval v;
            v = OBJECT_TO_JSVAL(js_obj);
            JS_SetProperty(cx, glob, "foo", &v);

            char* txt[] = {
  "print('foo = '+foo);",
  "foo.toString = new Function('return \"foo toString called\";')",
  "foo.toStr = new Function('return \"foo toStr called\";')",
  "print('foo = '+foo);",
  "print('foo.toString() = '+foo.toString());",
  "print('foo.toStr() = '+foo.toStr());",
  "print('foo.five = '+ foo.five);",
  "print('foo.six = '+ foo.six);",
  "print('foo.bogus = '+ foo.bogus);",
  "foo.bogus = 5;",
  "print('foo.bogus = '+ foo.bogus);",
  "print('foo.Test(10,20) returned: '+foo.Test(10,20));",
  "function Test(p1, p2){print('test called in JS with p1 = '+p1+' and p2 = '+p2);return p1+p2;}",
  "bar = new Object();",
  "bar.Test = Test;",
//  "bar.Test(5,7);",
  "function QI(iid){print('QueryInterface called in JS with iid = '+iid); return  this;}",
  "bar.QueryInterface = QI;",
  "print('foo properties:');",
  "for(i in foo)print('  foo.'+i+' = '+foo[i]);",
  0,
            };

            for(char** p = txt; *p; p++)
                JS_EvaluateScript(cx, glob, *p, strlen(*p), "builtin", 1, &rval);

            if(JS_GetProperty(cx, glob, "bar", &v) && JSVAL_IS_OBJECT(v))
            {
                JSObject* bar = JSVAL_TO_OBJECT(v);
                nsIXPConnectWrappedJS* wrapper;
                if(NS_SUCCEEDED(xpc->WrapJS(cx,
                                       JSVAL_TO_OBJECT(v),
                                       kIFooIID, &wrapper)))
                {
                    nsITestXPCFoo* ptr = (nsITestXPCFoo*)wrapper;
                    int result;
                    JSObject* test_js_obj;
                    ptr->Test(11, 13, &result);
                    printf("call to ptr->Test returned %d\n", result);
    
                    nsIXPConnectWrappedJSMethods* methods;

                    wrapper->QueryInterface(kWrappedJSMethodsIID, 
                                            (void**) &methods);
                    methods->GetJSObject(&test_js_obj);

                    printf("call to methods->GetJSObject() returned: %s\n", 
                            test_js_obj == JSVAL_TO_OBJECT(v) ?
                            "expected result" : "WRONG RESULT" );

                    NS_RELEASE(methods);
                    NS_RELEASE(wrapper);

                }
            }
            NS_RELEASE(com_obj);
            NS_RELEASE(wrapper2);
        }
        NS_RELEASE(wrapper);
    }
    NS_RELEASE(foo);

    JS_GC(cx);
    JS_DestroyContext(cx);
    JS_DestroyRuntime(rt);
    JS_ShutDown();

    return 0;
}
