/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* Platform specific code to invoke XPCOM methods on native objects */

#include "xpcprivate.h"

static const char* XPC_QUERY_INTERFACE_STR = "QueryInterface";

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

NS_IMPL_ISUPPORTS(nsXPCWrappedJSClass, NS_IXPCONNECT_WRAPPED_JS_CLASS_IID)

// static
nsXPCWrappedJSClass*
nsXPCWrappedJSClass::GetNewOrUsedClass(XPCContext* xpcc,
                                       REFNSIID aIID)
{
    IID2WrappedJSClassMap* map;
    nsXPCWrappedJSClass* clazz = NULL;

    NS_PRECONDITION(xpcc, "bad param");

    map = xpcc->GetWrappedJSClassMap();
    NS_ASSERTION(map,"bad map");

    clazz = map->Find(aIID);
    if(clazz)
    {
        NS_ADDREF(clazz);
    }
    else
    {
        nsIInterfaceInfo* info;
        nsXPConnect* xpc;
        if((xpc = nsXPConnect::GetXPConnect()) != NULL &&
           NS_SUCCEEDED(xpc->GetInterfaceInfo(aIID, &info)))
        {
            clazz = new nsXPCWrappedJSClass(xpcc, aIID, info);
        }
    }
    return clazz;
}

nsXPCWrappedJSClass::nsXPCWrappedJSClass(XPCContext* xpcc, REFNSIID aIID,
                                         nsIInterfaceInfo* aInfo)
    : mXPCContext(xpcc),
      mIID(aIID),
      mInfo(aInfo)
{
    NS_ADDREF(mInfo);

    NS_INIT_REFCNT();
    NS_ADDREF_THIS();

    mXPCContext->GetWrappedJSClassMap()->Add(this);
}

nsXPCWrappedJSClass::~nsXPCWrappedJSClass()
{
    mXPCContext->GetWrappedJSClassMap()->Remove(this);
    NS_RELEASE(mInfo);
}

/***************************************************************************/
// XXX for now IIDs are represented in JS as string objects containing strings
// of the form: {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx} (just like the nsID
// Parse and ToString methods use.

// XXX lots of room for optimization here!

// JSObject*
// nsXPCWrappedJSClass::CreateIIDJSObject(REFNSIID aIID)
// {
//     JSObject* obj = NULL;
//     char* str = aIID.ToString();
//     if(str)
//     {
//         JSContext* cx = GetJSContext();
//         JSString* jsstr = JS_InternString(cx, str);
//         delete [] str;
//         if(jsstr)
//             JS_ValueToObject(cx, STRING_TO_JSVAL(jsstr), &obj);
//     }
//     return obj;
// }

JSObject*
nsXPCWrappedJSClass::CallQueryInterfaceOnJSObject(JSObject* jsobj, REFNSIID aIID)
{
    JSContext* cx = GetJSContext();
    JSObject* id;
    jsval retval;
    JSObject* retObj;
    JSBool success = JS_FALSE;

//    id = CreateIIDJSObject(aIID);
    id = xpc_NewIDObject(cx, aIID);

    if(id)
    {
        JSErrorReporter older = JS_SetErrorReporter(cx, NULL);
        jsval args[1] = {OBJECT_TO_JSVAL(id)};
        success = JS_CallFunctionName(cx, jsobj, XPC_QUERY_INTERFACE_STR,
                                      1, args, &retval);
        if(success)
            success = JS_ValueToObject(cx, retval, &retObj);
        JS_SetErrorReporter(cx, older);
    }

    return success ? retObj : NULL;
}

/***************************************************************************/
// This 'WrappedJSIdentity' class and singleton allow us to figure out if
// any given nsISupports* is implemented by a WrappedJS object. This is done
// using a QueryInterface call on the interface pointer with our ID. If
// that call returns NS_OK and the pointer is to our singleton, then the
// interface must be implemented by a WrappedJS object. NOTE: the
// 'WrappedJSIdentity' object is not a real XPCOM object and should not be
// used for anything else (hence it is declared in this implementation file).

// {5C5C3BB0-A9BA-11d2-BA64-00805F8A5DD7}
#define NS_IXPCONNECT_WRAPPED_JS_IDENTITY_CLASS_IID \
{ 0x5c5c3bb0, 0xa9ba, 0x11d2,                       \
  { 0xba, 0x64, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7 } }

static NS_DEFINE_IID(kIWrappedJSIdentityIID, NS_IXPCONNECT_WRAPPED_JS_IDENTITY_CLASS_IID);

class WrappedJSIdentity
{
    // no instance methods...
public:
    static void* GetSingleton()
    {
        static WrappedJSIdentity* singleton = NULL;
        if(!singleton)
            singleton = new WrappedJSIdentity();
        return (void*) singleton;
    }
};

/***************************************************************************/

JSBool
nsXPCWrappedJSClass::IsWrappedJS(nsISupports* aPtr)
{
    void* result;
    NS_PRECONDITION(aPtr, "null pointer");
    return aPtr &&
           NS_OK == aPtr->QueryInterface(kIWrappedJSIdentityIID, &result) &&
           result == WrappedJSIdentity::GetSingleton();
}

nsresult
nsXPCWrappedJSClass::DelegatedQueryInterface(nsXPCWrappedJS* self,
                                             REFNSIID aIID,
                                             void** aInstancePtr)
{
    if(NULL == aInstancePtr)
    {
        NS_PRECONDITION(0, "null pointer");
        return NS_ERROR_NULL_POINTER;
    }

    if(aIID.Equals(kISupportsIID))
    {
        nsXPCWrappedJS* root = self->GetRootWrapper();
        *aInstancePtr = (void*) root;
        NS_ADDREF(root);
        return NS_OK;
    }
    else if(aIID.Equals(self->GetIID()))
    {
        *aInstancePtr = (void*) self;
        NS_ADDREF(self);
        return NS_OK;
    }
    else if(aIID.Equals(kIWrappedJSIdentityIID))
    {
        *aInstancePtr = WrappedJSIdentity::GetSingleton();
        return NS_OK;
    }
    else
    {
        JSObject* jsobj =
            CallQueryInterfaceOnJSObject(self->GetJSObject(), aIID);
        if(jsobj)
        {
            nsXPCWrappedJS* wrapper =
                nsXPCWrappedJS::GetNewOrUsedWrapper(GetXPCContext(),
                                                    jsobj, aIID);
            if(wrapper)
            {
                *aInstancePtr = (void*) wrapper;
                return NS_OK;
            }
        }
    }

    *aInstancePtr = NULL;
    return NS_NOINTERFACE;
}

JSObject*
nsXPCWrappedJSClass::GetRootJSObject(JSObject* aJSObj)
{
    JSObject* result = CallQueryInterfaceOnJSObject(aJSObj, kISupportsIID);
    return result ? result : aJSObj;
}

#define JAM_DOUBLE(v,d) (d = (jsdouble)v, DOUBLE_TO_JSVAL(&d))
#define FIT_32(i,d) (INT_FITS_IN_JSVAL(i) ? INT_TO_JSVAL(i) : JAM_DOUBLE(i,d))
// Win32 can't handle uint64 to double conversion
#define JAM_DOUBLE_U64(v,d) JAM_DOUBLE(((int64)v),d)

nsresult
nsXPCWrappedJSClass::CallMethod(nsXPCWrappedJS* wrapper,
                                const nsXPCMethodInfo* info,
                                nsXPCMiniVarient* params)
{
#define ARGS_BUFFER_COUNT     32

    jsval argsBuffer[ARGS_BUFFER_COUNT];
    jsval* argv = NULL;
    uint8 i;
    uint8 argc;
    jsval result;
    uint8 paramCount;
    nsresult retval = NS_ERROR_UNEXPECTED;
    JSErrorReporter older;
    JSBool success;
    JSContext* cx = GetJSContext();


    // XXX ASSUMES that retval is last arg.
    paramCount = info->GetParamCount();
    argc = paramCount -
            (paramCount && info->GetParam(paramCount-1).IsRetval() ? 1 : 0);

    // XXX do we need to go through and NULL out all 'out' params?

    // setup argv
    if(argc > ARGS_BUFFER_COUNT)
    {
        if(!(argv = new jsval[argc]))
            return NS_ERROR_OUT_OF_MEMORY;
    }
    else
        argv = argsBuffer;
    // from here on out we exit via 'goto done'

    // build the args
    for(i = 0; i < argc; i++)
    {
        const nsXPCParamInfo& param = info->GetParam(i);
        const nsXPCType& type = param.GetType();

        jsval val;

        if(param.IsIn())
        {
            nsXPCMiniVarient* pv;

            if(param.IsOut())
                pv = (nsXPCMiniVarient*) params[i].val.p;
            else
                pv = &params[i];

            if(type & nsXPCType::IS_POINTER)
                pv = (nsXPCMiniVarient*) pv->val.p;

            // do the actual conversion...

            // handle special cases first

            if(type == nsXPCType::T_INTERFACE)
            {
                // XXX implement INTERFACE

                // make sure 'src' is an object
                // get the nsIInterfaceInfo* from the param and
                // build a wrapper and then hand over the wrapper.
                // XXX remember to release the wrapper in cleanup below

                NS_ASSERTION(0,"interface params not supported");
                continue;
            }
            else if(type == nsXPCType::T_INTERFACE_IS)
            {
                // XXX implement INTERFACE_IS
                NS_ASSERTION(0,"interface_is params not supported");
                continue;
            }
            else if(type == nsXPCType::T_STRING)
            {
                // XXX implement STRING
                NS_ASSERTION(0,"string params not supported");
                continue;
            }
            else if(type == nsXPCType::T_P_IID)
            {
                // XXX implement IID
                NS_ASSERTION(0,"iid params not supported");
                continue;
            }
            else if(type == nsXPCType::T_P_VOID)
            {
                // XXX implement void*
                NS_ASSERTION(0,"void* params not supported");
                continue;
            }
            else {
                jsdouble d;

                switch(type & nsXPCType::TYPE_MASK)
                {
                case nsXPCType::T_I8     : val = INT_TO_JSVAL((int32)pv->val.i8);   break;
                case nsXPCType::T_I16    : val = INT_TO_JSVAL((int32)pv->val.i16);  break;
                case nsXPCType::T_I32    : val = FIT_32(pv->val.i32,d);             break;
                case nsXPCType::T_I64    : val = JAM_DOUBLE(pv->val.i64,d);         break;
                case nsXPCType::T_U8     : val = INT_TO_JSVAL((int32)pv->val.u8);   break;
                case nsXPCType::T_U16    : val = INT_TO_JSVAL((int32)pv->val.u16);  break;
                case nsXPCType::T_U32    : val = FIT_32(pv->val.u32,d);             break;
                case nsXPCType::T_U64    : val = JAM_DOUBLE_U64(pv->val.u64,d);     break;
                case nsXPCType::T_FLOAT  : val = JAM_DOUBLE(pv->val.f,d);           break;
                case nsXPCType::T_DOUBLE : val = DOUBLE_TO_JSVAL(&pv->val.d);       break;
                case nsXPCType::T_BOOL   : val = pv->val.b?JSVAL_TRUE:JSVAL_FALSE;  break;
                // XXX need to special case cahr* and wchar_t*
                case nsXPCType::T_CHAR   : val = INT_TO_JSVAL((int32)pv->val.c);    break;
                case nsXPCType::T_WCHAR  : val = INT_TO_JSVAL((int32)pv->val.wc);   break;
                default:
                    NS_ASSERTION(0, "bad type");
                    continue;
                }
            }
        }

        if(param.IsOut())
        {
            // create an 'out' object
            JSObject* obj = NewOutObject();
            if(param.IsIn())
            {
                if(!JS_SetProperty(cx, obj, XPC_VAL_STR, &val))
                    goto done;
            }
            argv[i] = OBJECT_TO_JSVAL(obj);
        }
        else
            argv[i] = val;
    }

    // do the function call

    older = JS_SetErrorReporter(cx, NULL);
    success = JS_CallFunctionName(cx, wrapper->GetJSObject(), info->GetName(),
                                  argc, argv, &result);
    JS_SetErrorReporter(cx, older);
    if(!success)
    {
        retval = NS_ERROR_FAILURE;
        goto done;
    }

    // convert out args and result
    // NOTE: this is the total number of native params, not just the args
    for(i = 0; i < paramCount; i++)
    {
        const nsXPCParamInfo& param = info->GetParam(i);

        if(param.IsOut())
        {
            jsval val;
            const nsXPCType& type = param.GetType();
            nsXPCMiniVarient* pv;

            if(param.IsRetval())
                val = result;
            else if(!JS_GetProperty(cx, JSVAL_TO_OBJECT(argv[i]), XPC_VAL_STR, &val))
                goto done;

            pv = (nsXPCMiniVarient*) params[i].val.p;
            if(type & nsXPCType::IS_POINTER)
                pv = (nsXPCMiniVarient*) pv->val.p;

            // do the actual conversion...

            // handle special cases first

            if(type == nsXPCType::T_INTERFACE)
            {
                // XXX implement INTERFACE

                // make sure 'src' is an object
                // get the nsIInterfaceInfo* from the param and
                // build a wrapper and then hand over the wrapper.
                // XXX remember to release the wrapper in cleanup below

                NS_ASSERTION(0,"interface params not supported");
                continue;
            }
            else if(type == nsXPCType::T_INTERFACE_IS)
            {
                // XXX implement INTERFACE_IS
                NS_ASSERTION(0,"interface_is params not supported");
                continue;
            }
            else if(type == nsXPCType::T_STRING)
            {
                // XXX implement STRING
                NS_ASSERTION(0,"string params not supported");
                continue;
            }
            else if(type == nsXPCType::T_P_IID)
            {
                // XXX implement IID
                NS_ASSERTION(0,"iid params not supported");
                continue;
            }
            else if(type == nsXPCType::T_P_VOID)
            {
                // XXX implement void*
                NS_ASSERTION(0,"void* params not supported");
                continue;
            }
            else {
                int32    ti;
                uint32   tu;
                jsdouble td;
                JSBool   r;

                switch(type & nsXPCType::TYPE_MASK)
                {
                case nsXPCType::T_I8     :
                    r = JS_ValueToECMAInt32(cx,val,&ti);
                    pv->val.i8  = (int8) ti;
                    break;
                case nsXPCType::T_I16    :
                    r = JS_ValueToECMAInt32(cx,val,&ti);
                    pv->val.i16  = (int16) ti;
                    break;
                case nsXPCType::T_I32    :
                    r = JS_ValueToECMAInt32(cx,val,&pv->val.i32);
                    break;
                case nsXPCType::T_I64    :
                    if(JSVAL_IS_INT(val))
                    {
                        r = JS_ValueToECMAInt32(cx,val,&ti);
                        pv->val.i64 = (int64) ti;
                    }
                    else
                    {
                        r = JS_ValueToNumber(cx, val, &td);
                        if(r) pv->val.i64 = (int64) td;
                    }
                    break;
                case nsXPCType::T_U8     :
                    r = JS_ValueToECMAUint32(cx,val,&tu);
                    pv->val.u8  = (uint8) tu;
                    break;
                case nsXPCType::T_U16    :
                    r = JS_ValueToECMAUint32(cx,val,&tu);
                    pv->val.u16  = (uint16) tu;
                    break;
                case nsXPCType::T_U32    :
                    r = JS_ValueToECMAUint32(cx,val,&pv->val.u32);
                    break;
                case nsXPCType::T_U64    :
                    if(JSVAL_IS_INT(val))
                    {
                        r = JS_ValueToECMAUint32(cx,val,&tu);
                        pv->val.i64 = (int64) tu;
                    }
                    else
                    {
                        r = JS_ValueToNumber(cx, val, &td);
                        // XXX Win32 can't handle double to uint64 directly
                        if(r) pv->val.u64 = (uint64)((int64) td);
                    }
                    break;
                case nsXPCType::T_FLOAT  :
                    r = JS_ValueToNumber(cx, val, &td);
                    if(r) pv->val.f = (float) td;
                    break;
                case nsXPCType::T_DOUBLE :
                    r = JS_ValueToNumber(cx, val, &pv->val.d);
                    break;
                case nsXPCType::T_BOOL   :
                    r = JS_ValueToBoolean(cx, val, &pv->val.b);
                    break;

                // XXX should we special case char* and wchar_t* to be strings?
                case nsXPCType::T_CHAR   :
                case nsXPCType::T_WCHAR  :
                default:
                    NS_ASSERTION(0, "bad type");
                    goto done;
                }
                continue;
            }
        }
    }

    retval = NS_OK;

done:
    if(argv && argv != argsBuffer)
        delete [] argv;
    return retval;
}


static JSClass WrappedJSOutArg_class = {
    "XPCOutArg", 0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

// static
JSBool
nsXPCWrappedJSClass::InitForContext(XPCContext* xpcc)
{
    if (!JS_InitClass(xpcc->GetJSContext(), xpcc->GetGlobalObject(),
        0, &WrappedJSOutArg_class, 0, 0,
        0, 0,
        0, 0))
        return JS_FALSE;
    return JS_TRUE;
}

JSObject*
nsXPCWrappedJSClass::NewOutObject()
{
    return JS_NewObject(GetJSContext(), &WrappedJSOutArg_class, NULL, NULL);
}
