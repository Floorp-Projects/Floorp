#include "JSWrapper.h"
#include "../prassert.h"

extern const JS_PUBLIC_DATA(nsIID) kIScriptableIID = JS_ISCRIPTABLE_IID;

static JSBool
JS_IScriptableGetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_STRING(id))
		return JS_FALSE;
	// do ints later
	jsIScriptable* jsi = (jsIScriptable*)JS_GetPrivate(cx,obj);
	PR_ASSERT(jsi);
	JSString *jstr = JSVAL_TO_STRING(id);
	char* prop = JS_GetStringBytes(jstr);	
	return (jsi->get(cx,prop,vp) == NS_OK ? JS_TRUE : JS_FALSE);
}

JSBool
JS_IScriptableSetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_STRING(id))
		return JS_FALSE;
	// do ints later
	jsIScriptable* jsi = (jsIScriptable*)JS_GetPrivate(cx,obj);
	PR_ASSERT(jsi);
	JSString *jstr = JSVAL_TO_STRING(id);
	char* prop = JS_GetStringBytes(jstr);	
	return (jsi->put(cx,prop,*vp) == NS_OK ? JS_TRUE : JS_FALSE);
}

static void
Scriptable_Finalize(JSContext *cx, JSObject *obj)
{
	jsIScriptable* jsi;

	jsi = (jsIScriptable*)JS_GetPrivate(cx,obj);
	if (jsi)
		jsi->Release();
}

static JSClass IScriptableWrapper_class = {
    "IScriptableWrapper",
    JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE,
    JS_PropertyStub,JS_PropertyStub,  
	JS_IScriptableGetProperty,JS_IScriptableSetProperty,
    JS_EnumerateStub,JS_ResolveStub, 
	JS_ConvertStub,Scriptable_Finalize
};

JS_PUBLIC_API(nsresult)
jsIScriptable::FromJS(JSContext* cx, jsval v, double* d)
{
	if (JSVAL_IS_DOUBLE(v)) {
		*d = *JSVAL_TO_DOUBLE(v);
		return NS_OK;
	}
	else if (JSVAL_IS_INT(v)) {
		int i = JSVAL_TO_INT(v);
		*d = (double)i;
		return NS_OK;
	}
	else
		return NS_ERROR_FAILURE;
}

JS_PUBLIC_API(nsresult)
jsIScriptable::FromJS(JSContext* cx, jsval v, char** s, size_t n)
{
	if (JSVAL_IS_STRING(v)) {
		JSString *jstr = JSVAL_TO_STRING(v);
		char* str = JS_GetStringBytes(jstr);
		strncpy(*s, str, n);
		return NS_OK;
	}
	else
		return NS_ERROR_FAILURE;
}

JS_PUBLIC_API(nsresult)
jsIScriptable::FromJS(JSContext* cx, jsval v, jsIScriptable** o)
{
	if (JSVAL_IS_OBJECT(v)) {
		JSObject* jobj = JSVAL_TO_OBJECT(v);
		JSClass* jclass = JS_GetClass(cx,jobj);
		if (strcmp(jclass->name,"IScriptableWrapper") == 0) {
			*o = (jsIScriptable*)JS_GetPrivate(cx,jobj);
			return NS_OK;
		}
		else {
			*o = (jsIScriptable*)new JSWrapper(cx,jobj);
			if (*o == NULL)
				return NS_ERROR_FAILURE;
			else
				return NS_OK;
		}
	}
	else
		return NS_ERROR_FAILURE;
}

static JSBool
MethodCall(JSContext *cx,JSObject *obj,uintN argc,jsval *argv,jsval *vp)
{
	JSObject* meth_obj;
	void* p;

 	meth_obj = JSVAL_TO_OBJECT(argv[-2]);
	p = JS_GetPrivate(cx,meth_obj);
	PR_ASSERT(p);
	Meth* meth = (Meth*)p;
	return (meth->invoke(argc,argv,vp) == NS_OK ? JS_TRUE : JS_FALSE);
}

static JSBool
Method_Convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
	*vp = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static void
Method_Finalize(JSContext *cx, JSObject *obj)
{
	Meth* meth;

	meth = (Meth*)JS_GetPrivate(cx,obj);
	if (meth)
		meth->SubRef();
}

static JSBool
Method_lookupProperty(JSContext *cx, JSObject *obj, jsid id,
                         JSObject **objp, JSProperty **propp
#if defined JS_THREADSAFE && defined DEBUG
                            , const char *file, uintN line
#endif
			    )
{
    return JS_TRUE;
}

static JSBool
Method_defineProperty(JSContext *cx, JSObject *obj, jsid id, jsval value,
                         JSPropertyOp getter, JSPropertyOp setter,
                         uintN attrs, JSProperty **propp)
{
    return JS_FALSE;
}

static JSBool
Method_getAttributes(JSContext *cx, JSObject *obj, jsid id,
                        JSProperty *prop, uintN *attrsp)
{
    return JS_FALSE;
}

static JSBool
Method_setAttributes(JSContext *cx, JSObject *obj, jsid id,
                        JSProperty *prop, uintN *attrsp)
{
    return JS_TRUE;
}

static JSBool
Method_deleteProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    return JS_FALSE;
}

static JSBool
Method_defaultValue(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
Method_getProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
Method_setProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	return JS_TRUE;
}

static JSBool
Method_newEnumerate(JSContext *cx,JSObject *obj,JSIterateOp enum_op,jsval *statep,jsid *idp)
{
	return JS_TRUE;
}

static JSBool
Method_checkAccess(JSContext *cx, JSObject *obj, jsid id,
		       JSAccessMode mode, jsval *vp, uintN *attrsp)
{
	return JS_TRUE;
}

JSBool MethodCall(JSContext*,JSObject*,uintN,jsval*,jsval*);

JSObjectOps Method_ops = {
    /* Mandatory non-null function pointer members. */
    NULL,     /* newObjectMap */
	NULL,     /* destroyObjectMap */
    Method_lookupProperty,
    Method_defineProperty,
    Method_getProperty,  
    Method_setProperty,
    Method_getAttributes,
    Method_setAttributes,
    Method_deleteProperty,
    Method_defaultValue,
    Method_newEnumerate,
    Method_checkAccess,
    /* Optionally non-null members start here. */
    NULL,                       /* thisObject */
    NULL,                       /* dropProperty */
    MethodCall,                 /* call */
    NULL,                       /* construct */
    NULL,                       /* xdrObject */
    NULL,                       /* hasInstance */
};

extern "C" PR_IMPORT_DATA(JSObjectOps) js_ObjectOps;

JSObjectOps *
Method_getObjectOps(JSContext *cx, JSClass *clazz)
{
	if (Method_ops.newObjectMap == NULL) {
		Method_ops.newObjectMap = js_ObjectOps.newObjectMap;
		Method_ops.destroyObjectMap = js_ObjectOps.destroyObjectMap;
	}
    return &Method_ops;
}

static JSClass MethodWrapper_class = {
    "MethodWrapper", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   Method_Convert,   Method_Finalize,
    Method_getObjectOps,
};


JS_PUBLIC_API(nsresult)
jsIScriptable::ToJS(JSContext* cx, double d, jsval* v)
{
	return (JS_NewDoubleValue(cx,d,v) == JS_TRUE ? NS_OK : NS_ERROR_FAILURE);
}

JS_PUBLIC_API(nsresult)
jsIScriptable::ToJS(JSContext* cx, char* s, jsval* v)
{
	JSString* jstr = JS_NewStringCopyZ(cx, s);
	if (jstr == NULL)
		return NS_ERROR_FAILURE;
	*v = STRING_TO_JSVAL(jstr);
	return NS_OK;
}

JS_PUBLIC_API(nsresult)
jsIScriptable::ToJS(JSContext* cx, jsIScriptable* jsi, jsval* v)
{
	JSObject* jobj = jsi->GetJS();
	if (jobj == NULL) {
		jobj = JS_NewObject(cx,&IScriptableWrapper_class,NULL,NULL);
		if (jobj == NULL)
			return NS_ERROR_FAILURE;
		jsi->SetJS(jobj);
		jsi->AddRef();
		JS_SetPrivate(cx,jobj,(void*)jsi);
	}
	*v = OBJECT_TO_JSVAL(jobj);
	return NS_OK;
}

JS_PUBLIC_API(nsresult)
jsIScriptable::ToJS(JSContext* cx, Meth* meth, jsval* vp)
{
	jsval v = meth->GetJSVal(cx);
	if (v) {
		*vp = v;
		return NS_OK;
	}
	JSObject* meth_obj = JS_NewObject(cx,&MethodWrapper_class,NULL,NULL);
	if (meth_obj == NULL)
		return NS_ERROR_FAILURE;
	*vp = OBJECT_TO_JSVAL(meth_obj);
	JS_SetPrivate(cx,meth_obj,(void*)meth);
	meth->SetJSVal(cx,*vp);
	meth->SetJSContext(cx);
	meth->AddRef();
	return NS_OK;
}
