
#include "nsIXPConnect.h"
#include "jsapi.h"
#include <stdio.h>

/***************************************************************************/
// {159E36D0-991E-11d2-AC3F-00C09300144B}
#define NS_ITESTXPC_FOO_IID       \
{ 0x159e36d0, 0x991e, 0x11d2,   \
  { 0xac, 0x3f, 0x0, 0xc0, 0x93, 0x0, 0x14, 0x4b } }

class nsITestXPCFoo : public nsISupports
{
    public:
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
    nsTestXPCFoo();
};

nsresult nsTestXPCFoo::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    // XXX bogus trust...
    *aInstancePtr = (void*) this;
    NS_ADDREF_THIS();
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

    nsIXPConnect* xpc = XPC_GetXPConnect();
    nsIJSContext* xpccx = XPC_NewJSContext(cx);
    nsIJSObject* xpcglob = XPC_NewJSObject(xpccx, glob);


    nsTestXPCFoo* foo = new nsTestXPCFoo();
    
    nsIXPConnectWrappedNative* wrapper;
    nsIXPConnectWrappedNative* wrapper2;
    if(NS_SUCCEEDED(xpc->WrapNative(xpccx, foo, kIFooIID, &wrapper)))
    {
        if(NS_SUCCEEDED(xpc->WrapNative(xpccx, foo, kIFoo2IID, &wrapper2)))
        {
            nsIJSObject* obj;
            nsISupports* com_obj;

            xpc->GetJSObjectOfWrappedNative(wrapper2, &obj);
            xpc->GetNativeOfWrappedNative(wrapper2, &com_obj);

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
