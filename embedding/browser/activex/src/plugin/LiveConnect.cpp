/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adam Lock <adamlock@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#include "stdafx.h"

#include "jni.h"
#include "npapi.h"

#include "_java/java_lang_Throwable.h"
#include "_java/java_lang_Error.h"
#include "_java/java_lang_String.h"
#include "_java/java_lang_Boolean.h"
#include "_java/java_lang_Number.h"
#include "_java/java_lang_Integer.h"
#include "_java/java_lang_Long.h"
// TODO:
// These things require certain native statics to be implemented
// so they're commented out for the time being.
//#include "_java/java_lang_Double.h"
//#include "_java/java_lang_Float.h"
#include "_java/java_lang_Character.h"
#include "_java/netscape_plugin_Plugin.h"
#include "_java/MozAxPlugin.h"

#include "LegacyPlugin.h"
#include "LiveConnect.h"

void liveconnect_Shutdown()
{
    JRIEnv* env = NPN_GetJavaEnv();
    if (env) {
        unuse_MozAxPlugin(env);
        unuse_netscape_plugin_Plugin(env);
        unuse_java_lang_Error(env);
//        unuse_java_lang_String(env);
        unuse_java_lang_Number(env);
        unuse_java_lang_Boolean(env);
        unuse_java_lang_Integer(env);
        unuse_java_lang_Long(env);
//        unuse_java_lang_Float(env);
//        unuse_java_lang_Double(env);
        unuse_java_lang_Character(env);
    }
}

jref liveconnect_GetJavaClass()
{
    JRIEnv* env = NPN_GetJavaEnv();
    if (env) {
        // Note: The order of these is important (for some unknown reason)
        //       and was determined through trial and error. Do not rearrange
        //       without testing the new order!
        use_netscape_plugin_Plugin(env);
        jref myClass = (jref) use_MozAxPlugin(env);
        use_java_lang_Error(env);
//        use_java_lang_String(env);
        use_java_lang_Number(env);
        use_java_lang_Boolean(env);
        use_java_lang_Integer(env);
        use_java_lang_Long(env);
//        use_java_lang_Float(env);
//        use_java_lang_Double(env);
        use_java_lang_Character(env);
        return myClass;
    }
    return NULL;
}

// The following will be callable from Javascript through LiveConnect
HRESULT
_GetIDispatchFromJRI(JRIEnv *env, struct MozAxPlugin* self, IDispatch **pdisp)
{
    if (pdisp == NULL || env == NULL || self == NULL)
    {
        return E_INVALIDARG;
    }
    *pdisp = NULL;

    // Note: You get a nasty crash calling self->getPeer(env), this obscure cast fixes
    //       it. More details in the link:
    //
    // http://groups.google.com/groups?selm=385D9543.4087F1C6%40ermapper.com.au&output=gplain

    NPP npp = (NPP) netscape_plugin_Plugin_getPeer(env,
            reinterpret_cast<netscape_plugin_Plugin*> (self));
    PluginInstanceData *pData = (PluginInstanceData *) npp->pdata;
    if (pData == NULL)
    { 
        return E_FAIL;
    }

    IUnknownPtr unk;
    HRESULT hr = pData->pControlSite->GetControlUnknown(&unk);
    if (unk.GetInterfacePtr() == NULL)
    {
        return E_FAIL; 
    }

    IDispatchPtr disp = unk;
    if (disp.GetInterfacePtr() == NULL)
    { 
        return E_FAIL; 
    }

    *pdisp = disp.GetInterfacePtr();
    (*pdisp)->AddRef();

    return S_OK;
}

HRESULT
_VariantToJRIObject(JRIEnv *env, VARIANT *v, java_lang_Object **o)
{
    if (v == NULL || env == NULL || o == NULL)
    {
        return E_INVALIDARG;
    }

    *o = NULL;

    // TODO - VT_BYREF will cause problems
    if (v->vt == VT_EMPTY)
    {
        return S_OK;
    }
    else if (v->vt == VT_BOOL)
    {
        jbool value = (v->boolVal == VARIANT_TRUE) ? JRITrue : JRIFalse;
        java_lang_Boolean *j = java_lang_Boolean_new(env,
            class_java_lang_Boolean(env), value);
        *o = reinterpret_cast<java_lang_Object *>(j);
        return S_OK;
    }
    else if (v->vt == VT_I4)
    {
        jlong value = v->lVal;
        java_lang_Long *j = java_lang_Long_new(env,
            class_java_lang_Long(env), value);
        *o = reinterpret_cast<java_lang_Object *>(j);
        return S_OK;
    }
    else if (v->vt == VT_I2)
    {
        jlong value = v->iVal;
        java_lang_Long *j = java_lang_Long_new(env,
            class_java_lang_Long(env), value);
        *o = reinterpret_cast<java_lang_Object *>(j);
        return S_OK;
    }
/*  else if (v->vt == VT_R4)
    {
        jfloat value = v->fltVal;
        java_lang_Float *j = java_lang_Float_new(env,
            class_java_lang_Float(env), value);
        *o = reinterpret_cast<java_lang_Object *>(j);
        return S_OK;
    }
    else if (v->vt == VT_R8)
    {
        jdouble value = v->dblVal;
        java_lang_Double *j = java_lang_Double_new(env,
            class_java_lang_Double(env), value);
        *o = reinterpret_cast<java_lang_Object *>(j);
        return S_OK;
    } */
    else if (v->vt == VT_BSTR)
    {
        USES_CONVERSION;
        char * value = OLE2A(v->bstrVal);
        java_lang_String *j = JRI_NewStringUTF(env, value, strlen(value));
        *o = reinterpret_cast<java_lang_Object *>(j);
        return S_OK;
    }
    /* TODO else if VT_UI1 etc. */

    return E_FAIL;
}

HRESULT
_JRIObjectToVariant(JRIEnv *env, java_lang_Object *o, VARIANT *v)
{
    VariantInit(v);
    if (JRI_IsInstanceOf(env, (jref) o, class_java_lang_String(env)))
    {
        USES_CONVERSION;
        const char *value = JRI_GetStringUTFChars(env, reinterpret_cast<java_lang_String *>(o));
        v->vt = VT_BSTR;
        v->bstrVal = SysAllocString(A2COLE(value));
    }
    else if (JRI_IsInstanceOf(env, (jref) o, class_java_lang_Boolean(env)))
    {
        jbool value = java_lang_Boolean_booleanValue(env, reinterpret_cast<java_lang_Boolean *>(o));
        v->vt = VT_BOOL;
        v->boolVal = value == JRITrue ? VARIANT_TRUE : VARIANT_FALSE;
    }
    else if (JRI_IsInstanceOf(env, o, class_java_lang_Integer(env)))
    {
        jint value = java_lang_Integer_intValue(env, reinterpret_cast<java_lang_Integer *>(o));
        v->vt = VT_I4;
        v->lVal = value;
    }
    else if (JRI_IsInstanceOf(env, o, class_java_lang_Long(env)))
    {
        jlong value = java_lang_Long_longValue(env, reinterpret_cast<java_lang_Long *>(o));
        v->vt = VT_I4;
        v->lVal = value;
    }
/*  else if (JRI_IsInstanceOf(env, o, class_java_lang_Double(env)))
    {
        jdouble  value = java_lang_Double_doubleValue(env, reinterpret_cast<java_lang_Double *>(o));
        v->vt = VT_R8;
        v->dblVal = value;
    }
    else if (JRI_IsInstanceOf(env, o, class_java_lang_Float(env)))
    {
        jfloat value = java_lang_Float_floatValue(env, reinterpret_cast<java_lang_Float *>(o));
        v->vt = VT_R4;
        v->fltVal = value;
    } */
    else if (JRI_IsInstanceOf(env, o, class_java_lang_Character(env)))
    {
        jchar value = java_lang_Character_charValue(env, reinterpret_cast<java_lang_Character *>(o));
        v->vt = VT_UI1;
        v->bVal = value;
    }
    else if (JRI_IsInstanceOf(env, o, class_java_lang_Number(env)))
    {
        jlong value = java_lang_Number_longValue(env, reinterpret_cast<java_lang_Number *>(o));
        v->vt = VT_I4;
        v->lVal = value;
    }
    else
    {
        // TODO dump diagnostic error here
        return E_FAIL;
    }
    return S_OK;
}

struct java_lang_Object *
_InvokeFromJRI(JRIEnv *env, struct MozAxPlugin* self, struct java_lang_String *func, int nargs, java_lang_Object *args[])
{
    HRESULT hr;
    DISPID dispid = 0;
    
    // call the requested function
    const char* funcName = JRI_GetStringUTFChars(env, func);

    IDispatchPtr disp;
    if (FAILED(_GetIDispatchFromJRI(env, self, &disp)))
    {
        return NULL;
    }

    _variant_t *vargs = new _variant_t[nargs];
    for (int i = 0; i < nargs; i++)
    {
        if (FAILED(_JRIObjectToVariant(env, args[i], &vargs[i])))
        {
            delete []vargs;
            char error[64];
            sprintf(error, "Argument %d could not be converted into a variant", i);
            JRI_ThrowNew(env, class_java_lang_Error(env), error); 
            return NULL;
        }
    }

    USES_CONVERSION;
    OLECHAR FAR* szMember = A2OLE(funcName);
    hr = disp->GetIDsOfNames(IID_NULL, &szMember, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr))
    { 
        char error[128];
        memset(error, 0, sizeof(error));
        _snprintf(error, sizeof(error) - 1, "invoke failed, member \"%s\" not found, hr=0x%08x", funcName, hr);
        JRI_ThrowNew(env, class_java_lang_Error(env), error); 
        return NULL; 
    }

    DISPPARAMS dispparams = {NULL, NULL, 0, 0};
    dispparams.rgvarg = vargs;
    dispparams.cArgs = nargs;

    _variant_t vResult;
    hr = disp->Invoke(
        dispid,
        IID_NULL,
        LOCALE_USER_DEFAULT,
        DISPATCH_METHOD,
        &dispparams, &vResult, NULL, NULL);
    
    if (FAILED(hr))
    { 
        char error[64];
        sprintf(error, "invoke failed, result from object = 0x%08x", hr);
        JRI_ThrowNew(env, class_java_lang_Error(env), error); 
        return NULL; 
    }

    java_lang_Object *oResult = NULL;
    _VariantToJRIObject(env, &vResult, &oResult);

    return reinterpret_cast<java_lang_Object *>(oResult);
}

/*******************************************************************************
 * Native Methods: 
 * These are the native methods which we are implementing.
 ******************************************************************************/

/*** private native xinvoke (Ljava/lang/String;)Ljava/lang/Object; ***/
extern "C" JRI_PUBLIC_API(struct java_lang_Object *)
native_MozAxPlugin_xinvoke(JRIEnv* env, struct MozAxPlugin* self, struct java_lang_String *a)
{
    return _InvokeFromJRI(env, self, a, 0, NULL);
}

/*** private native xinvoke1 (Ljava/lang/String;Ljava/lang/Object;)Ljava/lang/Object; ***/
extern "C" JRI_PUBLIC_API(struct java_lang_Object *)
native_MozAxPlugin_xinvoke1(JRIEnv* env, struct MozAxPlugin* self, struct java_lang_String *a, struct java_lang_Object *b)
{
    java_lang_Object *args[1];
    args[0] = b;
    return _InvokeFromJRI(env, self, a, sizeof(args) / sizeof(args[0]), args);
}

/*** private native xinvoke2 (Ljava/lang/String;Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object; ***/
extern "C" JRI_PUBLIC_API(struct java_lang_Object *)
native_MozAxPlugin_xinvoke2(JRIEnv* env, struct MozAxPlugin* self, struct java_lang_String *a, struct java_lang_Object *b, struct java_lang_Object *c)
{
    java_lang_Object *args[2];
    args[0] = b;
    args[1] = c;
    return _InvokeFromJRI(env, self, a, sizeof(args) / sizeof(args[0]), args);
}

/*** private native xinvoke3 (Ljava/lang/String;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object; ***/
extern "C" JRI_PUBLIC_API(struct java_lang_Object *)
native_MozAxPlugin_xinvoke3(JRIEnv* env, struct MozAxPlugin* self, struct java_lang_String *a, struct java_lang_Object *b, struct java_lang_Object *c, struct java_lang_Object *d)
{
    java_lang_Object *args[3];
    args[0] = b;
    args[1] = c;
    args[2] = d;
    return _InvokeFromJRI(env, self, a, sizeof(args) / sizeof(args[0]), args);
}

/*** private native xinvoke4 (Ljava/lang/String;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object; ***/
extern "C" JRI_PUBLIC_API(struct java_lang_Object *)
native_MozAxPlugin_xinvoke4(JRIEnv* env, struct MozAxPlugin* self, struct java_lang_String *a, struct java_lang_Object *b, struct java_lang_Object *c, struct java_lang_Object *d, struct java_lang_Object *e)
{
    java_lang_Object *args[4];
    args[0] = b;
    args[1] = c;
    args[2] = d;
    args[3] = e;
    return _InvokeFromJRI(env, self, a, sizeof(args) / sizeof(args[0]), args);
}

/*** private native xinvokeX (Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object; ***/
extern "C" JRI_PUBLIC_API(struct java_lang_Object *)
native_MozAxPlugin_xinvokeX(JRIEnv* env, struct MozAxPlugin* self, struct java_lang_String *a, jobjectArray b)
{
    // Turn the Java array of objects into a C++ array
    jsize length = JRI_GetObjectArrayLength(env, b);
    java_lang_Object **args = NULL;
    if (length)
    {
        args = (java_lang_Object **) malloc(length * sizeof(java_lang_Object *));
        for (long i = 0; i < length; i++)
        {
             args[i] = reinterpret_cast<java_lang_Object *>(JRI_GetObjectArrayElement(env, b, i));
        }
    }
    java_lang_Object *o = _InvokeFromJRI(env, self, a, length, args);
    free(args);
    return o;
}

/*** private native xgetProperty (Ljava/lang/String;)Ljava/lang/Object; ***/
extern "C" JRI_PUBLIC_API(struct java_lang_Object *)
native_MozAxPlugin_xgetProperty(JRIEnv* env, struct MozAxPlugin* self, struct java_lang_String *a)
{
    HRESULT hr;
    DISPID dispid;
    _variant_t vResult;

    IDispatchPtr disp;
    if (FAILED(_GetIDispatchFromJRI(env, self, &disp)))
    {
        return NULL;
    }

    // return the requested property to the Java peer
    USES_CONVERSION;
    OLECHAR FAR* szMember = A2OLE(JRI_GetStringUTFChars(env, a));
    hr = disp->GetIDsOfNames(IID_NULL, &szMember, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr))
    { 
        char error[128];
        memset(error, 0, sizeof(error));
        _snprintf(error, sizeof(error) - 1, "getProperty failed, member \"%s\" not found, hr=0x%08x", szMember, hr);
        JRI_ThrowNew(env, class_java_lang_Error(env), error); 
        return NULL; 
    }
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    hr = disp->Invoke(
        dispid,
        IID_NULL,
        LOCALE_USER_DEFAULT,
        DISPATCH_PROPERTYGET,
        &dispparamsNoArgs, &vResult, NULL, NULL);
    if (FAILED(hr))
    { 
        char error[64];
        sprintf(error, "getProperty failed, result from object = 0x%08x", hr);
        JRI_ThrowNew(env, class_java_lang_Error(env), error); 
        return NULL; 
    }

    java_lang_Object *oResult = NULL;
    _VariantToJRIObject(env, &vResult, &oResult);

    return oResult;
}

/*** private native xsetProperty2 (Ljava/lang/String;Ljava/lang/Object;)V ***/
extern "C" JRI_PUBLIC_API(void)
native_MozAxPlugin_xsetProperty2(JRIEnv* env, struct MozAxPlugin* self, struct java_lang_String *a, struct java_lang_Object *b)

{
    HRESULT hr;
    DISPID dispid;
    VARIANT VarResult;

    IDispatchPtr disp;
    if (FAILED(_GetIDispatchFromJRI(env, self, &disp)))
    {
        return;
    }

    USES_CONVERSION;
    OLECHAR FAR* szMember = A2OLE(JRI_GetStringUTFChars(env, a));
    hr = disp->GetIDsOfNames(IID_NULL, &szMember, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr))
    { 
        char error[128];
        memset(error, 0, sizeof(error));
        _snprintf(error, sizeof(error) - 1, "setProperty failed, member \"%s\" not found, hr=0x%08x", szMember, hr);
        JRI_ThrowNew(env, class_java_lang_Error(env), error); 
        return;
    }

    _variant_t *pvars = new _variant_t[1];
    if (FAILED(_JRIObjectToVariant(env, b, &pvars[0])))
    {
        delete []pvars;
        char error[64];
        sprintf(error, "Property value could not be converted into a variant");
        JRI_ThrowNew(env, class_java_lang_Error(env), error); 
        return;
    }

    DISPID dispIdPut = DISPID_PROPERTYPUT;

    DISPPARAMS functionArgs;
    functionArgs.rgdispidNamedArgs = &dispIdPut;
    functionArgs.rgvarg = pvars;
    functionArgs.cArgs = 1;
    functionArgs.cNamedArgs = 1;

    hr = disp->Invoke(
        dispid,
        IID_NULL,
        LOCALE_USER_DEFAULT,
        DISPATCH_PROPERTYPUT,
        &functionArgs, &VarResult, NULL, NULL);

    delete []pvars;
    
    if (FAILED(hr))
    {
        char error[64];
        sprintf(error, "setProperty failed, result from object = 0x%08x", hr);
        JRI_ThrowNew(env, class_java_lang_Error(env), error); 
        return;
    }
}

/*** private native xsetProperty1 (Ljava/lang/String;Ljava/lang/String;)V ***/
extern "C" JRI_PUBLIC_API(void)
native_MozAxPlugin_xsetProperty1(JRIEnv* env, struct MozAxPlugin* self, struct java_lang_String *a, struct java_lang_String *b)
{
    native_MozAxPlugin_xsetProperty2(env, self, a, reinterpret_cast<java_lang_Object *>(b));
}


///////////////////////////////////////////////////////////////////////////////

/*** private native printStackTrace0 (Ljava/io/PrintStream;)V ***/
extern "C" JRI_PUBLIC_API(void)
native_java_lang_Throwable_printStackTrace0(JRIEnv* env, struct java_lang_Throwable* self, struct java_io_PrintStream *a)
{
}

/*** public native fillInStackTrace ()Ljava/lang/Throwable; ***/
extern "C" JRI_PUBLIC_API(struct java_lang_Throwable *)
native_java_lang_Throwable_fillInStackTrace(JRIEnv* env, struct java_lang_Throwable* self)
{
    return self;
}
