
#include "nsIXPConnect.h"
#include "jsapi.h"
#include <stdio.h>

#include "xpcbogusii.h"


/***************************************************************************/
// {159E36D0-991E-11d2-AC3F-00C09300144B}
#define NS_ITESTXPC_FOO_IID       \
{ 0x159e36d0, 0x991e, 0x11d2,   \
  { 0xac, 0x3f, 0x0, 0xc0, 0x93, 0x0, 0x14, 0x4b } }

class nsITestXPCFoo : public nsISupports
{
public:
    NS_IMETHOD Test(int p1, int p2) = 0;
    NS_IMETHOD Test2() = 0;
};

// {5F9D20C0-9B6B-11d2-9FFE-000064657374}
#define NS_ITESTXPC_FOO2_IID       \
{ 0x5f9d20c0, 0x9b6b, 0x11d2,     \
  { 0x9f, 0xfe, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }

class nsITestXPCFoo2 : public nsITestXPCFoo
{
public:
};


class nsTestXPCFoo : public nsITestXPCFoo2
{
    NS_DECL_ISUPPORTS;
    NS_IMETHOD Test(int p1, int p2);
    NS_IMETHOD Test2();
    nsTestXPCFoo();
};

nsresult nsTestXPCFoo::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    // XXX bogus trust...
    *aInstancePtr = (void*) this;
    NS_ADDREF_THIS();
    return NS_OK;
}

nsresult nsTestXPCFoo::Test(int p1, int p2)
{
    printf("nsTestXPCFoo::Test called with p1 = %d and p2 = %d\n", p1, p2);
    return NS_OK;
}
nsresult nsTestXPCFoo::Test2()
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

NS_DEFINE_IID(kIFooIID, NS_ITESTXPC_FOO_IID);
NS_DEFINE_IID(kIFoo2IID, NS_ITESTXPC_FOO2_IID);


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
    nsIJSContext* xpccx = XPC_NewJSContext(cx);
    nsIJSObject* xpcglob = XPC_NewJSObject(xpccx, glob);

    xpc->InitJSContext(xpccx, xpcglob);

    nsTestXPCFoo* foo = new nsTestXPCFoo();

//    nsXPCVarient v[2];
//    v[0].type = nsXPCType::T_I32; v[0].val.i32 = 1;
//    v[1].type = nsXPCType::T_I32; v[1].val.i32 = 2;

//    XPC_TestInvoke(foo, 3, 2, v);
//    XPC_TestInvoke(foo, 4, 0, NULL);

    nsIXPConnectWrappedNative* wrapper;
    nsIXPConnectWrappedNative* wrapper2;
    if(NS_SUCCEEDED(xpc->WrapNative(xpccx, foo, kIFooIID, &wrapper)))
    {
        if(NS_SUCCEEDED(xpc->WrapNative(xpccx, foo, kIFoo2IID, &wrapper2)))
        {
            JSObject* jsobj;
            nsIJSObject* obj;
            nsISupports* com_obj;
            jsval rval;

            xpc->GetJSObjectOfWrappedNative(wrapper2, &obj);
            xpc->GetNativeOfWrappedNative(wrapper2, &com_obj);

            obj->GetNative(&jsobj);

            jsval v;
            v = OBJECT_TO_JSVAL(jsobj);
            JS_SetProperty(cx, glob, "foo", &v);

            char* txt[] = {
                "print('foo.five = '+ foo.five)",
                "print('foo.six = '+ foo.six)",
                "print('foo.bogus = '+ foo.bogus)",
                "foo.Test(10,20)",
                0,
            };

            for(char** p = txt; *p; p++)
                JS_EvaluateScript(cx, glob, *p, strlen(*p), "builtin", 1, &rval);

            NS_RELEASE(obj);
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
