
#include "nsIXPConnect.h"
#include "nsIXPCScriptable.h"
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIXPCScriptable.h"
#include "nsIServiceManager.h"
#include "jsapi.h"
#include <stdio.h>
#include "xpclog.h"


// XXX this should not be necessary, but the nsIAllocator service is not being 
// started right now.
#include "nsAllocator.h"
#include "nsRepository.h"

/***************************************************************************/
/***************************************************************************/
// copying in the contents of nsAllocator.cpp as a test...

static NS_DEFINE_IID(kIAllocatorIID, NS_IALLOCATOR_IID);
static NS_DEFINE_IID(kAllocatorCID, NS_ALLOCATOR_CID);

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
        aIID.Equals(nsISupports::IID())) {
        *aInstancePtr = (void*) this; 
        AddRef(); 
        return NS_OK; 
    } 
    return NS_NOINTERFACE;
}

NS_METHOD
nsAllocator::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    if (outer && !aIID.Equals(nsISupports::IID()))
        return NS_NOINTERFACE;   // XXX right error?
    nsAllocator* mm = new nsAllocator(outer);
    if (mm == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    mm->AddRef();
    if (aIID.Equals(nsISupports::IID()))
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
    nsRepository::RegisterFactory(kAllocatorCID, NULL, NULL,
                                  (nsIFactory*)new nsAllocatorFactory(),
                                  PR_FALSE);
}    


/***************************************************************************/
class MyScriptable : public nsIXPCScriptable
{
    NS_DECL_ISUPPORTS;
    XPC_DECLARE_IXPCSCRIPTABLE;
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

  if (aIID.Equals(nsITestXPCFoo::IID()) || 
      aIID.Equals(nsITestXPCFoo2::IID()) ||
      aIID.Equals(nsISupports::IID())) {
    *aInstancePtr = (void*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIXPCScriptable::IID())) {
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

/***************************************************************************/

class MyEcho : public nsIEcho
{
public:
    NS_DECL_ISUPPORTS;
    NS_IMETHOD SetReciever(nsIEcho* aReciever);
    NS_IMETHOD SendOneString(const char* str);
    NS_IMETHOD In2OutOneInt(int input, int* output);
    NS_IMETHOD In2OutAddTwoInts(int input1, 
                                int input2,
                                int* output1,
                                int* output2,
                                int* result);
    NS_IMETHOD In2OutOneString(const char* input, char** output);
    NS_IMETHOD SimpleCallNoEcho();
    NS_IMETHOD SendManyTypes(int8    p1,
                             int16   p2,
                             int32   p3,
                             int64   p4,
                             uint8   p5,
                             uint16  p6,
                             uint32  p7,
                             uint64  p8,
                             float   p9,
                             double  p10,
                             PRBool  p11,
                             char    p12,
                             uint16  p13,
                             nsID*   p14,
                             char*   p15,
                             uint16* p16);
    NS_IMETHOD SendInOutManyTypes(int8*    p1,
                                  int16*   p2,
                                  int32*   p3,
                                  int64*   p4,
                                  uint8*   p5,
                                  uint16*  p6,
                                  uint32*  p7,
                                  uint64*  p8,
                                  float*   p9,
                                  double*  p10,
                                  PRBool*  p11,
                                  char*    p12,
                                  uint16*  p13,
                                  nsID**   p14,
                                  char**   p15,
                                  uint16** p16);
    NS_IMETHOD MethodWithNative(int p1, void* p2);

    MyEcho();
private: 
    nsIEcho* mReciever;
    nsIAllocator* mAllocator;
};

static NS_DEFINE_IID(kMyEchoIID, NS_IECHO_IID);
NS_IMPL_ISUPPORTS(MyEcho, kMyEchoIID);

MyEcho::MyEcho() 
    : mReciever(NULL)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
    nsServiceManager::GetService(kAllocatorCID,
                                 kIAllocatorIID,
                                 (nsISupports **)&mAllocator);
}

NS_IMETHODIMP MyEcho::SetReciever(nsIEcho* aReciever)
{
    if(mReciever)
        NS_RELEASE(mReciever);
    mReciever = aReciever;
    if(mReciever)
        NS_ADDREF(mReciever);
    return NS_OK;
}

NS_IMETHODIMP MyEcho::SendOneString(const char* str)
{
    if(mReciever)
        return mReciever->SendOneString(str);
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
MyEcho::SendManyTypes(int8    p1,
                      int16   p2,
                      int32   p3,
                      int64   p4,
                      uint8   p5,
                      uint16  p6,
                      uint32  p7,
                      uint64  p8,
                      float   p9,
                      double  p10,
                      PRBool  p11,
                      char    p12,
                      uint16  p13,
                      nsID*   p14,
                      char*   p15,
                      uint16* p16)
{
    if(mReciever)
        return mReciever->SendManyTypes(p1, p2, p3, p4, p5, p6, p7, p8, p9,
                                        p10, p11, p12, p13, p14, p15, p16);
    return NS_OK;
}    

NS_IMETHODIMP 
MyEcho::SendInOutManyTypes(int8*    p1,
                           int16*   p2,
                           int32*   p3,
                           int64*   p4,
                           uint8*   p5,
                           uint16*  p6,
                           uint32*  p7,
                           uint64*  p8,
                           float*   p9,
                           double*  p10,
                           PRBool*  p11,
                           char*    p12,
                           uint16*  p13,
                           nsID**   p14,
                           char**   p15,
                           uint16** p16)
{
    if(mReciever)
        return mReciever->SendInOutManyTypes(p1, p2, p3, p4, p5, p6, p7, p8, p9,
                                             p10, p11, p12, p13, p14, p15, p16);
    return NS_OK;
}

NS_IMETHODIMP 
MyEcho::MethodWithNative(int p1, void* p2)
{
    return NS_OK;
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

int main()
{
    JSRuntime *rt;
    JSContext *cx;
    JSObject *glob;

    gErrFile = stderr;
    gOutFile = stdout;

    RegAllocator();

    rt = JS_NewRuntime(8L * 1024L * 1024L);
    if (!rt)
        return 1;
    cx = JS_NewContext(rt, 8192);
    if (!cx)
        return 1;

    JS_SetErrorReporter(cx, my_ErrorReporter);

    nsIXPConnect* xpc = XPC_GetXPConnect();

/*
    // old code where global object was plain object
    glob = JS_NewObject(cx, &global_class, NULL, NULL);
    if (!glob)
        return 1;

    if (!JS_InitStandardClasses(cx, glob))
        return 1;
    if (!JS_DefineFunctions(cx, glob, glob_functions))
        return 1;

    xpc->InitJSContext(cx, glob);
*/

    nsTestXPCFoo* foo = new nsTestXPCFoo();

//    nsXPCVarient v[2];
//    v[0].type = nsXPCType::T_I32; v[0].val.i32 = 1;
//    v[1].type = nsXPCType::T_I32; v[1].val.i32 = 2;

//    XPC_TestInvoke(foo, 3, 2, v);
//    XPC_TestInvoke(foo, 4, 0, NULL);

    nsIXPConnectWrappedNative* wrapper;
    nsIXPConnectWrappedNative* wrapper2;
/*
    if(NS_SUCCEEDED(xpc->WrapNative(cx, foo, nsITestXPCFoo::IID(), &wrapper)))
*/
    // new code where global object is a wrapped xpcom object
    if(NS_SUCCEEDED(xpc->InitJSContextWithNewWrappedGlobal(
                                    cx, foo, nsITestXPCFoo::IID(), &wrapper)))
    {
        wrapper->GetJSObject(&glob);
        JS_DefineFunctions(cx, glob, glob_functions);

        if(NS_SUCCEEDED(xpc->WrapNative(cx, foo, nsITestXPCFoo2::IID(), &wrapper2)))
        {
            JSObject* js_obj;
            nsISupports* com_obj;
            jsval rval;

            wrapper2->GetJSObject(&js_obj);
            wrapper2->GetNative(&com_obj);

            jsval v;
            v = OBJECT_TO_JSVAL(js_obj);
            JS_SetProperty(cx, glob, "foo", &v);

            // add the reflected native echo object (sans error checking :)
            nsIXPConnectWrappedNative* echo_wrapper;
            JSObject* echo_jsobj;
            jsval echo_jsval;
            xpc->WrapNative(cx, new MyEcho(), nsIEcho::IID(), &echo_wrapper);
            echo_wrapper->GetJSObject(&echo_jsobj);
            echo_jsval = OBJECT_TO_JSVAL(echo_jsobj);
            JS_SetProperty(cx, glob, "echo", &echo_jsval);

            char* txt[] = {
              "load('testxpc.js');",
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
                                       nsITestXPCFoo::IID(), &wrapper)))
                {
                    nsITestXPCFoo* ptr = (nsITestXPCFoo*)wrapper;
                    int result;
                    JSObject* test_js_obj;
                    ptr->Test(11, 13, &result);
                    printf("call to ptr->Test returned %d\n", result);
    
                    nsIXPConnectWrappedJSMethods* methods;

                    wrapper->QueryInterface(nsIXPConnectWrappedJSMethods::IID(), 
                                            (void**) &methods);
                    methods->GetJSObject(&test_js_obj);

                    printf("call to methods->GetJSObject() returned: %s\n", 
                            test_js_obj == JSVAL_TO_OBJECT(v) ?
                            "expected result" : "WRONG RESULT" );

                    // dump to log test...
                    XPC_DUMP(xpc, 50);

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

    // XXX things are still rooted to the global object in the test scripts...

    // dump to log test...
//    XPC_LOG_ALWAYS((""));
//    XPC_LOG_ALWAYS(("after running release object..."));
//    XPC_LOG_ALWAYS((""));
//    XPC_DUMP(xpc, 3);

    JS_GC(cx);

    // dump to log test...
//    XPC_LOG_ALWAYS((""));
//    XPC_LOG_ALWAYS(("after running JS_GC..."));
//    XPC_LOG_ALWAYS((""));
//    XPC_DUMP(xpc, 3);

    JS_DestroyContext(cx);
    JS_DestroyRuntime(rt);
    JS_ShutDown();
    return 0;
}

/***************************************************************************/

#include "jsatom.h"
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
void Dobj(nsISupports* p, int depth) {if(p)XPC_DUMP(p,depth);}
void Dxpc(int depth) {Dobj(XPC_GetXPConnect(), depth);}
JS_END_EXTERN_C
