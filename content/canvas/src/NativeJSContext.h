#ifndef _NATIVEJSCONTEXT_H_
#define _NATIVEJSCONTEXT_H_

class JSObjectHelper;

class NativeJSContext {
public:
    NativeJSContext() {
        error = gXPConnect->GetCurrentNativeCallContext(&ncc);
        if (NS_FAILED(error))
            return;

        if (!ncc) {
            error = NS_ERROR_FAILURE;
            return;
        }

        ctx = nsnull;

        error = ncc->GetJSContext(&ctx);
        if (NS_FAILED(error))
            return;

        JS_BeginRequest(ctx);

        ncc->GetArgc(&argc);
        ncc->GetArgvPtr(&argv);
    }

    ~NativeJSContext() {
        JS_EndRequest(ctx);
    }

    PRBool CheckArray (JSObject *obj, jsuint *sz) {
        if (obj &&
            ::JS_IsArrayObject(ctx, obj) &&
            ::JS_GetArrayLength(ctx, obj, sz))
            return PR_TRUE;
        return PR_FALSE;
    }

    PRBool CheckArray (jsval val, jsuint *sz) {
        if (!JSVAL_IS_NULL(val) &&
            JSVAL_IS_OBJECT(val) &&
            ::JS_IsArrayObject(ctx, JSVAL_TO_OBJECT(val)) &&
            ::JS_GetArrayLength(ctx, JSVAL_TO_OBJECT(val), sz))
            return PR_TRUE;
        return PR_FALSE;
    }

    PRBool AddGCRoot (void *aPtr, const char *aName) {
        return JS_AddNamedRootRT(gScriptRuntime, aPtr, aName);
    }

    void ReleaseGCRoot (void *aPtr) {
        JS_RemoveRootRT(gScriptRuntime, aPtr);
    }

    void SetRetVal (PRInt32 val) {
        if (INT_FITS_IN_JSVAL(val))
            SetRetValAsJSVal(INT_TO_JSVAL(val));
        else
            SetRetVal((double) val);
    }

    void SetRetVal (PRUint32 val) {
        if (INT_FITS_IN_JSVAL(val))
            SetRetValAsJSVal(INT_TO_JSVAL((int) val));
        else
            SetRetVal((double) val);
    }

    void SetRetVal (double val) {
        jsval *vp;
        ncc->GetRetValPtr(&vp);
        JS_NewDoubleValue(ctx, val, vp);
    }

    void SetBoolRetVal (PRBool val) {
        if (val)
            SetRetValAsJSVal(JSVAL_TRUE);
        else
            SetRetValAsJSVal(JSVAL_FALSE);
    }

    void SetRetVal (PRInt32 *vp, PRUint32 len) {
        nsAutoArrayPtr<jsval> jsvector(new jsval[len]);

        if (!JS_EnterLocalRootScope(ctx))
            return; // XXX ???

        for (PRUint32 i = 0; i < len; i++) {
            if (INT_FITS_IN_JSVAL(vp[i])) {
                jsvector[i] = INT_TO_JSVAL(vp[i]);
            } else {
                JS_NewDoubleValue(ctx, vp[i], &jsvector[i]);
            }
        }

        JSObject *jsarr = JS_NewArrayObject(ctx, len, jsvector.get());
        SetRetVal(jsarr);

        JS_LeaveLocalRootScope(ctx);
    }

    void SetRetVal (PRUint32 *vp, PRUint32 len) {
        nsAutoArrayPtr<jsval> jsvector(new jsval[len]);

        if (!JS_EnterLocalRootScope(ctx))
            return; // XXX ???

        for (PRUint32 i = 0; i < len; i++) {
            JS_NewNumberValue(ctx, vp[i], &jsvector[i]);
        }

        JSObject *jsarr = JS_NewArrayObject(ctx, len, jsvector.get());
        SetRetVal(jsarr);

        JS_LeaveLocalRootScope(ctx);
    }

    void SetRetVal (double *dp, PRUint32 len) {
        nsAutoArrayPtr<jsval> jsvector(new jsval[len]);

        if (!JS_EnterLocalRootScope(ctx))
            return; // XXX ???

        for (PRUint32 i = 0; i < len; i++)
            JS_NewDoubleValue(ctx, (jsdouble) dp[i], &jsvector[i]);
            
        JSObject *jsarr = JS_NewArrayObject(ctx, len, jsvector.get());
        SetRetVal(jsarr);

        JS_LeaveLocalRootScope(ctx);
    }

    void SetRetVal (float *fp, PRUint32 len) {
        nsAutoArrayPtr<jsval> jsvector(new jsval[len]);

        if (!JS_EnterLocalRootScope(ctx))
            return; // XXX ???

        for (PRUint32 i = 0; i < len; i++)
            JS_NewDoubleValue(ctx, (jsdouble) fp[i], &jsvector[i]);
        JSObject *jsarr = JS_NewArrayObject(ctx, len, jsvector.get());
        SetRetVal(jsarr);

        JS_LeaveLocalRootScope(ctx);
    }

    void SetRetValAsJSVal (jsval val) {
        jsval *vp;
        ncc->GetRetValPtr(&vp);
        *vp = val;
        ncc->SetReturnValueWasSet(PR_TRUE);
    }

    void SetRetVal (JSObject *obj) {
        SetRetValAsJSVal(OBJECT_TO_JSVAL(obj));
    }

    void SetRetVal (JSObjectHelper& objh);

    nsAXPCNativeCallContext *ncc;
    nsresult error;
    JSContext *ctx;
    PRUint32 argc;
    jsval *argv;

public:
    // static JS helpers

    static inline PRBool JSValToFloatArray (JSContext *ctx, jsval val,
                                            jsuint cnt, float *array)
    {
        JSObject *arrayObj;
        jsuint arrayLen;
        jsval jv;
        jsdouble dv;

        if (!::JS_ValueToObject(ctx, val, &arrayObj) ||
            arrayObj == NULL ||
            !::JS_IsArrayObject(ctx, arrayObj) ||
            !::JS_GetArrayLength(ctx, arrayObj, &arrayLen) ||
            (arrayLen < cnt))
            return PR_FALSE;

        for (jsuint i = 0; i < cnt; i++) {
            ::JS_GetElement(ctx, arrayObj, i, &jv);
            if (!::JS_ValueToNumber(ctx, jv, &dv))
                return PR_FALSE;
            array[i] = (float) dv;
        }

        return PR_TRUE;
    }

    static inline PRBool JSValToDoubleArray (JSContext *ctx, jsval val,
                                             jsuint cnt, double *array)
    {
        JSObject *arrayObj;
        jsuint arrayLen;
        jsval jv;
        jsdouble dv;

        if (!::JS_ValueToObject(ctx, val, &arrayObj) ||
            arrayObj == NULL ||
            !::JS_IsArrayObject(ctx, arrayObj) ||
            !::JS_GetArrayLength(ctx, arrayObj, &arrayLen) ||
            (arrayLen < cnt))
            return PR_FALSE;

        for (jsuint i = 0; i < cnt; i++) {
            ::JS_GetElement(ctx, arrayObj, i, &jv);
            if (!::JS_ValueToNumber(ctx, jv, &dv))
                return PR_FALSE;
            array[i] = dv;
        }

        return PR_TRUE;
    }

    static inline PRBool JSValToJSArrayAndLength (JSContext *ctx, jsval val,
                                                  JSObject **outObj, jsuint *outLen)
    {
        JSObject *obj = nsnull;
        jsuint len;
        if (!::JS_ValueToObject(ctx, val, &obj) ||
            obj == NULL ||
            !::JS_IsArrayObject(ctx, obj) ||
            !::JS_GetArrayLength(ctx, obj, &len))
        {
            return PR_FALSE;
        }

        *outObj = obj;
        *outLen = len;

        return PR_TRUE;
    }

    template<class T>
    static nsresult JSValToSpecificInterface(JSContext *ctx, jsval val, T **out)
    {
        if (JSVAL_IS_NULL(val)) {
            *out = nsnull;
            return NS_OK;
        }

        if (!JSVAL_IS_OBJECT(val))
            return NS_ERROR_DOM_SYNTAX_ERR;

        nsCOMPtr<nsISupports> isup;
        nsresult rv = gXPConnect->WrapJS(ctx, JSVAL_TO_OBJECT(val),
                                         NS_GET_IID(nsISupports),
                                         getter_AddRefs(isup));
        if (NS_FAILED(rv))
            return NS_ERROR_DOM_SYNTAX_ERR;

        nsCOMPtr<T> obj = do_QueryInterface(isup);
        if (!obj)
            return NS_ERROR_DOM_SYNTAX_ERR;

        NS_ADDREF(*out = obj.get());
        return NS_OK;
    }

    static inline JSObject *ArrayToJSArray (JSContext *ctx,
                                            const PRInt32 *vals,
                                            const PRUint32 len)
    {
        // XXX handle ints that are too big to fit
        nsAutoArrayPtr<jsval> jsvector(new jsval[len]);
        for (PRUint32 i = 0; i < len; i++)
            jsvector[i] = INT_TO_JSVAL(vals[i]);
        return JS_NewArrayObject(ctx, len, jsvector);
    }

    static inline JSObject *ArrayToJSArray (JSContext *ctx,
                                            const PRUint32 *vals,
                                            const PRUint32 len)
    {
        // XXX handle ints that are too big to fit
        nsAutoArrayPtr<jsval> jsvector(new jsval[len]);
        for (PRUint32 i = 0; i < len; i++)
            jsvector[i] = INT_TO_JSVAL(vals[i]);
        return JS_NewArrayObject(ctx, len, jsvector);
    }

};

class JSObjectHelper {
    friend class NativeJSContext;
public:
    JSObjectHelper(NativeJSContext *jsctx)
        : mCtx (jsctx)
    {
        mObject = JS_NewObject(mCtx->ctx, NULL, NULL, NULL);
        if (!mObject)
            return;

        if (!mCtx->AddGCRoot(&mObject, "JSObjectHelperCanvas3D"))
            mObject = nsnull;
    }

    ~JSObjectHelper() {
        if (mObject && mCtx)
            mCtx->ReleaseGCRoot(&mObject);
    }

    PRBool DefineProperty(const char *name, PRInt32 val) {
        // XXX handle too big ints
        if (!JS_DefineProperty(mCtx->ctx, mObject, name, INT_TO_JSVAL(val), NULL, NULL, JSPROP_ENUMERATE))
            return PR_FALSE;
        return PR_TRUE;
    }

    PRBool DefineProperty(const char *name, PRUint32 val) {
        // XXX handle too big ints
        if (!JS_DefineProperty(mCtx->ctx, mObject, name, INT_TO_JSVAL((int)val), NULL, NULL, JSPROP_ENUMERATE))
            return PR_FALSE;
        return PR_TRUE;
    }

    PRBool DefineProperty(const char *name, double val) {
        jsval dv;

        if (!JS_NewDoubleValue(mCtx->ctx, val, &dv))
            return PR_FALSE;

        if (!JS_DefineProperty(mCtx->ctx, mObject, name, dv, NULL, NULL, JSPROP_ENUMERATE))
            return PR_FALSE;
        return PR_TRUE;
    }

    PRBool DefineProperty(const char *name, JSObject *val) {
        if (!JS_DefineProperty(mCtx->ctx, mObject, name, OBJECT_TO_JSVAL(val), NULL, NULL, JSPROP_ENUMERATE))
            return PR_FALSE;
        return PR_TRUE;
    }

    // Blah.  We can't name this DefineProperty also because PRBool is the same as PRInt32
    PRBool DefineBoolProperty(const char *name, PRBool val) {
        if (!JS_DefineProperty(mCtx->ctx, mObject, name, val ? JS_TRUE : JS_FALSE, NULL, NULL, JSPROP_ENUMERATE))
            return PR_FALSE;
        return PR_TRUE;
    }

    // We can't use ns*Substring, because we don't have internal linkage
#if 0
    PRBool DefineProperty(const char *name, const nsCSubstring& val) {
        JSString *jsstr = JS_NewStringCopyN(mCtx->ctx, val.BeginReading(), val.Length());
        if (!jsstr ||
            !JS_DefineProperty(mCtx->ctx, mObject, name, STRING_TO_JSVAL(jsstr), NULL, NULL, JSPROP_ENUMERATE))
            return PR_FALSE;
        return PR_TRUE;
    }

    PRBool DefineProperty(const char *name, const nsSubstring& val) {
        JSString *jsstr = JS_NewUCStringCopyN(mCtx->ctx, val.BeginReading(), val.Length());
        if (!jsstr ||
            !JS_DefineProperty(mCtx->ctx, mObject, name, STRING_TO_JSVAL(jsstr), NULL, NULL, JSPROP_ENUMERATE))
            return PR_FALSE;
        return PR_TRUE;
    }
#endif

    PRBool DefineProperty(const char *name, const char *val, PRUint32 len) {
        JSString *jsstr = JS_NewStringCopyN(mCtx->ctx, val, len);
        if (!jsstr ||
            !JS_DefineProperty(mCtx->ctx, mObject, name, STRING_TO_JSVAL(jsstr), NULL, NULL, JSPROP_ENUMERATE))
            return PR_FALSE;
        return PR_TRUE;
    }

    JSObject *Object() {
        return mObject;
    }

protected:
    NativeJSContext *mCtx;
    JSObject *mObject;
};

inline void
NativeJSContext::SetRetVal(JSObjectHelper& objh) {
    SetRetValAsJSVal(OBJECT_TO_JSVAL(objh.mObject));
}

#endif
