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

/* Implement the nsID JavaScript class. */

#include "xpcprivate.h"

// IDData hold the private data
class IDData
{
public:
    IDData();   // not implemented
    IDData(const nsID& aID);
    IDData(JSContext *cx, const IDData& r);
    ~IDData();  // no virtual functions

    JSBool  GetString(JSContext *cx, jsval *rval);
    const nsID&  GetID() const {return mID;}
    JSBool  Equals(const IDData& r);
    void    Cleanup(JSContext *cx);
private:
    nsID      mID;
    JSString* mStr;
};

IDData::IDData(const nsID& aID) : mID(aID), mStr(NULL) {}

IDData::IDData(JSContext *cx, const IDData& r) : mID(r.mID), mStr(r.mStr)
{
    if(mStr)
        JS_AddRoot(cx, &mStr);
}
IDData::~IDData()
{
    NS_ASSERTION(!mStr, "deleting IDData without calling Cleanup first");
}

void
IDData::Cleanup(JSContext *cx)
{
    if(mStr)
    {
        JS_RemoveRoot(cx, &mStr);
        mStr = NULL;
    }
}

JSBool
IDData::GetString(JSContext *cx, jsval *rval)
{
    if(!mStr)
    {
        char* str = mID.ToString();
        NS_ASSERTION(str, "nsID.ToString failed");
        if(str)
        {
            if(NULL != (mStr = JS_NewStringCopyZ(cx, str)))
                JS_AddRoot(cx, &mStr);
            delete [] str;
        }
    }
    *rval = STRING_TO_JSVAL(mStr);
    return (JSBool) mStr;
}

JSBool
IDData::Equals(const IDData& r)
{
    return (JSBool) mID.Equals(r.mID);
}

/***************************************************************************/

JS_STATIC_DLL_CALLBACK(void)
nsID_finalize(JSContext *cx, JSObject *obj)
{
    IDData* data = (IDData*) JS_GetPrivate(cx, obj);
    if(data)
    {
        data->Cleanup(cx);
        delete data;
    }
}

static JSClass nsID_class = {
    "nsID",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   nsID_finalize
};

JS_STATIC_DLL_CALLBACK(JSBool)
nsID_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    IDData* data;

    if(!JS_InstanceOf(cx, obj, &nsID_class, NULL) ||
       !(data = (IDData*) JS_GetPrivate(cx, obj)))
        return JS_FALSE;

    return data->GetString(cx, rval);
}

JS_STATIC_DLL_CALLBACK(JSBool)
nsID_equals(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSObject *obj2;
    IDData* data1;
    IDData* data2;

    if(!JS_InstanceOf(cx, obj, &nsID_class, NULL) ||
       !argc || !JSVAL_IS_OBJECT(argv[0]) ||
       !(obj2 = JSVAL_TO_OBJECT(argv[0])) ||
       !JS_InstanceOf(cx, obj2, &nsID_class, NULL) ||
       !(data1 = (IDData*) JS_GetPrivate(cx, obj)) ||
       !(data2 = (IDData*) JS_GetPrivate(cx, obj2)))
        return JS_FALSE;

    return data1->Equals(*data2);
}

static JSFunctionSpec nsID_methods[] = {
    {"toString",    nsID_toString,  0},
    {"equals",      nsID_equals,    0},
    {0}
};

JS_STATIC_DLL_CALLBACK(JSBool)
nsID_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    IDData* data = NULL;
    JSString* str;
    nsID      id;

    if(argc == 0)
    {
        JS_ReportError(cx, "nsID has no constructor that takes zero arguments");
        return JS_FALSE;
    }

    if(JSVAL_IS_OBJECT(argv[0]) &&
       JS_InstanceOf(cx, JSVAL_TO_OBJECT(argv[0]), &nsID_class, NULL))
    {
        IDData* src = (IDData*)JS_GetPrivate(cx,JSVAL_TO_OBJECT(argv[0]));
        if(src)
            data = new IDData(cx, *src);
    }
    else if(NULL != (str = JS_ValueToString(cx, argv[0])) &&
            id.Parse(JS_GetStringBytes(str)))
    {
        data = new IDData(id);
    }

    if(!data)
    {
        JS_ReportError(cx, "could not constuct nsID");
        return JS_FALSE;
    }

    if(!JS_IsConstructing(cx))
    {
        obj = JS_NewObject(cx, &nsID_class, NULL, NULL);
        if (!obj)
            return JS_FALSE;
        *rval = OBJECT_TO_JSVAL(obj);
    }

    JS_SetPrivate(cx, obj, data);
    return JS_TRUE;
}

JSBool
xpc_InitIDClass(XPCContext* xpcc)
{
    if (!JS_InitClass(xpcc->GetJSContext(), xpcc->GetGlobalObject(),
        NULL, &nsID_class, nsID_ctor, 1, NULL, nsID_methods, NULL, NULL))
        return JS_FALSE;
    return JS_TRUE;
}

JSObject *
xpc_NewIDObject(JSContext *cx, const nsID& aID)
{
    JSObject *obj;

    obj = JS_NewObject(cx, &nsID_class, NULL, NULL);
    if(obj)
        JS_SetPrivate(cx, obj, new IDData(aID));
    return obj;
}

const nsID*
xpc_JSObjectToID(JSContext *cx, JSObject* obj)
{
    IDData* data;

    if(!cx || !obj ||
       !JS_InstanceOf(cx, obj, &nsID_class, NULL) ||
       !(data = (IDData*) JS_GetPrivate(cx, obj)))
        return NULL;

    return &data->GetID();
}        
