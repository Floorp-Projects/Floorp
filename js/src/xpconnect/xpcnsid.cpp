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
    static IDData* NewIDData(JSContext *cx, const nsID& id);
    static IDData* NewIDData(JSContext *cx, const IDData& data);
    static IDData* NewIDData(JSContext *cx, JSString* str);
    static void    DeleteIDData(JSContext *cx, IDData* data);

    void  GetNameString(JSContext *cx, jsval *rval);
    void  GetIDString(JSContext *cx, jsval *rval);
    const nsID&  GetID() const {return mID;}
    JSBool  Equals(const IDData& r);

    IDData();   // not implemented
    ~IDData();  // no virtual functions
private:
    IDData(JSContext *cx, const nsID& aID, 
           JSString* aIDString, JSString* aNameString);
private:
    static JSString* gNoString;
    nsID      mID;
    JSString* mIDString;
    JSString* mNameString;
};

/* static */ JSString* IDData::gNoString = (JSString*)"";

IDData::IDData(JSContext *cx, const nsID& aID, 
               JSString* aIDString, JSString* aNameString)
    : mID(aID), mIDString(aIDString), mNameString(aNameString)
{
    if(mNameString && mNameString != gNoString)
        JS_AddRoot(cx, &mNameString);

    if(mIDString && mIDString != gNoString)
        JS_AddRoot(cx, &mIDString);
}

// static 
IDData* 
IDData::NewIDData(JSContext *cx, const nsID& id)
{
    return new IDData(cx, id, NULL, NULL);
}        

// static 
IDData* 
IDData::NewIDData(JSContext *cx, const IDData& data)
{
    return new IDData(cx, data.mID, data.mIDString, data.mNameString);
}        

// static 
IDData* 
IDData::NewIDData(JSContext *cx, JSString* str)
{
    char* bytes;
    if(!str || !JS_GetStringLength(str) || !(bytes = JS_GetStringBytes(str)))
        return NULL;

    nsID id;
    if(bytes[0] == '{')
        if(id.Parse(bytes))
            return new IDData(cx, id, JS_NewStringCopyZ(cx, bytes), NULL);

    nsIInterfaceInfoManager* iim;
    if(!(iim = nsXPConnect::GetInterfaceInfoManager()))
        return NULL;

    IDData* data = NULL;
    nsID* pid;

    if(NS_SUCCEEDED(iim->GetIIDForName(bytes, &pid)) && pid)
    {
        data = new IDData(cx, *pid, NULL, JS_NewStringCopyZ(cx, bytes));

        nsIAllocator* al = nsXPConnect::GetAllocator();
        if(al)
        {
            al->Free(pid);
            NS_RELEASE(al);
        }
    }
    NS_RELEASE(iim);
    return data;
}        

IDData::~IDData() {}

// static 
void 
IDData::DeleteIDData(JSContext *cx, IDData* data)
{
    NS_PRECONDITION(cx, "bad JSContext");
    NS_PRECONDITION(data, "bad IDData");

    if(data->mNameString && data->mNameString != gNoString)
        JS_RemoveRoot(cx, &data->mNameString);

    if(data->mIDString && data->mIDString != gNoString)
        JS_RemoveRoot(cx, &data->mIDString);

    delete data;
}        

void
IDData::GetIDString(JSContext *cx, jsval *rval)
{
    if(!mIDString)
    {
        char* str = mID.ToString();
        if(str)
        {
            if(NULL != (mIDString = JS_NewStringCopyZ(cx, str)))
                JS_AddRoot(cx, &mIDString);
            delete [] str;
        }
        if(!mIDString)
            mIDString = gNoString;
    }
    if(mIDString == gNoString)
        *rval = JSVAL_NULL;
    else
        *rval = STRING_TO_JSVAL(mIDString);
}

void
IDData::GetNameString(JSContext *cx, jsval *rval)
{
    if(!mNameString)
    {
        nsIInterfaceInfoManager* iim;
        if(NULL != (iim = nsXPConnect::GetInterfaceInfoManager()))
        {
            char* name;
            if(NS_SUCCEEDED(iim->GetNameForIID(&mID, &name)) && name)
            {
                if(NULL != (mNameString = JS_NewStringCopyZ(cx, name)))
                    JS_AddRoot(cx, &mNameString);
                nsIAllocator* al = nsXPConnect::GetAllocator();
                if(al)
                {
                    al->Free(name);
                    NS_RELEASE(al);
                }
            }
            NS_RELEASE(iim);
        }
        if(!mNameString)
            mNameString = gNoString;
    }
    if(mNameString == gNoString)
        *rval = JSVAL_NULL;
    else
        *rval = STRING_TO_JSVAL(mNameString);
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
        IDData::DeleteIDData(cx, data);
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
    IDData* data=NULL;

    if(JS_InstanceOf(cx, obj, &nsID_class, NULL) &&
       NULL != (data = (IDData*) JS_GetPrivate(cx, obj)))
        data->GetIDString(cx, rval);
    else
        *rval = JSVAL_NULL;
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
nsID_toName(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    IDData* data=NULL;

    if(JS_InstanceOf(cx, obj, &nsID_class, NULL) &&
       NULL != (data = (IDData*) JS_GetPrivate(cx, obj)))
        data->GetNameString(cx, rval);
    else
        *rval = JSVAL_NULL;
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
nsID_isValid(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    IDData* data=NULL;

    if(JS_InstanceOf(cx, obj, &nsID_class, NULL) &&
       NULL != (data = (IDData*) JS_GetPrivate(cx, obj)))
        *rval = JSVAL_TRUE;
    else
        *rval = JSVAL_FALSE;
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
nsID_equals(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSObject *obj2;
    IDData* data1=NULL;
    IDData* data2=NULL;

    if(!JS_InstanceOf(cx, obj, &nsID_class, NULL) ||
       !argc || !JSVAL_IS_OBJECT(argv[0]) ||
       !(obj2 = JSVAL_TO_OBJECT(argv[0])) ||
       !JS_InstanceOf(cx, obj2, &nsID_class, NULL) ||
       !(data1 = (IDData*) JS_GetPrivate(cx, obj)) ||
       !(data2 = (IDData*) JS_GetPrivate(cx, obj2)))
        *rval = JSVAL_FALSE;
    else        
        *rval = data1->Equals(*data2) ? JSVAL_TRUE : JSVAL_FALSE;
    return JS_TRUE;
}

static JSFunctionSpec nsID_methods[] = {
    {"toString",    nsID_toString,  0},
    {"toName",      nsID_toName,    0},
    {"isValid",     nsID_isValid,   0},
    {"equals",      nsID_equals,    0},
    {0}
};

JS_STATIC_DLL_CALLBACK(JSBool)
nsID_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    IDData* data = NULL;
    JSString* str;

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
            data = IDData::NewIDData(cx, *src);
    }
    else if(NULL != (str = JS_ValueToString(cx, argv[0])))
    {
        data = IDData::NewIDData(cx, str);
    }

/*
*
* It does not work to return a null value from a ctor. Having the private set
* to null is THE indication that this object is not valid.
*
    if(!data)
    {
//        JS_ReportError(cx, "could not constuct nsID");
        *rval = JSVAL_NULL;
        return JS_TRUE;
    }
*/
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

    IDData* data = IDData::NewIDData(cx, aID);
    if(! data)
        return NULL;

    obj = JS_NewObject(cx, &nsID_class, NULL, NULL);
    if(obj)
        JS_SetPrivate(cx, obj, data);
    else
        IDData::DeleteIDData(cx, data);

    return obj;
}

const nsID*
xpc_JSObjectToID(JSContext *cx, JSObject* obj)
{
    if(!cx || !obj)
        return NULL;

    if(JS_InstanceOf(cx, obj, &nsID_class, NULL))
    {
        IDData* data = (IDData*) JS_GetPrivate(cx, obj);
        if(data)
            return &data->GetID();
        else
            return NULL;
    }
    // XXX it would be nice to construct one from an object that can be 
    // converted into a string. BUT since we return a const & we need to
    // have some storage space for the iid and that get complicated...
    return NULL;
}
