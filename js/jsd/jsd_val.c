/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

/*
 * JavaScript Debugging support - Value and Property support
 */

#include "jsd.h"

#ifdef DEBUG
void JSD_ASSERT_VALID_VALUE(JSDValue* jsdval)
{
    JS_ASSERT(jsdval);
    JS_ASSERT(jsdval->nref > 0);
    if(!JS_CLIST_IS_EMPTY(&jsdval->props))
    {
        JS_ASSERT(CHECK_BIT_FLAG(jsdval->flags, GOT_PROPS));
        JS_ASSERT(JSVAL_IS_OBJECT(jsdval->val));
    }

    if(jsdval->proto)
    {
        JS_ASSERT(CHECK_BIT_FLAG(jsdval->flags, GOT_PROTO));
        JS_ASSERT(jsdval->proto->nref > 0);
    }
    if(jsdval->parent)
    {
        JS_ASSERT(CHECK_BIT_FLAG(jsdval->flags, GOT_PARENT));
        JS_ASSERT(jsdval->parent->nref > 0);
    }
    if(jsdval->ctor)
    {
        JS_ASSERT(CHECK_BIT_FLAG(jsdval->flags, GOT_CTOR));
        JS_ASSERT(jsdval->ctor->nref > 0);
    }
}

void JSD_ASSERT_VALID_PROPERTY(JSDProperty* jsdprop)
{
    JS_ASSERT(jsdprop);
    JS_ASSERT(jsdprop->name);
    JS_ASSERT(jsdprop->name->nref > 0);
    JS_ASSERT(jsdprop->val);
    JS_ASSERT(jsdprop->val->nref > 0);
    if(jsdprop->alias)
        JS_ASSERT(jsdprop->alias->nref > 0);
}
#endif


JSBool
jsd_IsValueObject(JSDContext* jsdc, JSDValue* jsdval)
{
    return JSVAL_IS_OBJECT(jsdval->val);
}

JSBool
jsd_IsValueNumber(JSDContext* jsdc, JSDValue* jsdval)
{
    return JSVAL_IS_NUMBER(jsdval->val);
}

JSBool
jsd_IsValueInt(JSDContext* jsdc, JSDValue* jsdval)
{
    return JSVAL_IS_INT(jsdval->val);
}

JSBool
jsd_IsValueDouble(JSDContext* jsdc, JSDValue* jsdval)
{
    return JSVAL_IS_DOUBLE(jsdval->val);
}

JSBool
jsd_IsValueString(JSDContext* jsdc, JSDValue* jsdval)
{
    return JSVAL_IS_STRING(jsdval->val);
}

JSBool
jsd_IsValueBoolean(JSDContext* jsdc, JSDValue* jsdval)
{
    return JSVAL_IS_BOOLEAN(jsdval->val);
}

JSBool
jsd_IsValueNull(JSDContext* jsdc, JSDValue* jsdval)
{
    return JSVAL_IS_NULL(jsdval->val);
}

JSBool
jsd_IsValueVoid(JSDContext* jsdc, JSDValue* jsdval)
{
    return JSVAL_IS_VOID(jsdval->val);
}

JSBool
jsd_IsValuePrimitive(JSDContext* jsdc, JSDValue* jsdval)
{
    return JSVAL_IS_PRIMITIVE(jsdval->val);
}

JSBool
jsd_IsValueFunction(JSDContext* jsdc, JSDValue* jsdval)
{
    return !JSVAL_IS_PRIMITIVE(jsdval->val) &&
           JS_ObjectIsFunction(jsdc->dumbContext, JSVAL_TO_OBJECT(jsdval->val));
}

JSBool
jsd_IsValueNative(JSDContext* jsdc, JSDValue* jsdval)
{
    JSContext* cx = jsdc->dumbContext;
    jsval val = jsdval->val;
    JSFunction* fun;
    JSExceptionState* exceptionState;

    if(jsd_IsValueFunction(jsdc, jsdval))
    {
        exceptionState = JS_SaveExceptionState(cx);
        fun = JS_ValueToFunction(cx, val);
        JS_RestoreExceptionState(cx, exceptionState);
        if(!fun)
        {
            JS_ASSERT(0);
            return JS_FALSE;
        }
        return JS_GetFunctionScript(cx, fun) ? JS_FALSE : JS_TRUE;
    }
    return !JSVAL_IS_PRIMITIVE(val);
}

/***************************************************************************/

JSBool
jsd_GetValueBoolean(JSDContext* jsdc, JSDValue* jsdval)
{
    jsval val = jsdval->val;
    if(!JSVAL_IS_BOOLEAN(val))
        return JS_FALSE;
    return JSVAL_TO_BOOLEAN(val);
}

int32
jsd_GetValueInt(JSDContext* jsdc, JSDValue* jsdval)
{
    jsval val = jsdval->val;
    if(!JSVAL_IS_INT(val))
        return 0;
    return JSVAL_TO_INT(val);
}

jsdouble*
jsd_GetValueDouble(JSDContext* jsdc, JSDValue* jsdval)
{
    jsval val = jsdval->val;
    if(!JSVAL_IS_DOUBLE(val))
        return 0;
    return JSVAL_TO_DOUBLE(val);
}

JSString*
jsd_GetValueString(JSDContext* jsdc, JSDValue* jsdval)
{
    JSContext* cx = jsdc->dumbContext;
    JSExceptionState* exceptionState;

    if(!jsdval->string)
    {
        /* if the jsval is a string, then we don't need to double root it */
        if(JSVAL_IS_STRING(jsdval->val))
            jsdval->string = JSVAL_TO_STRING(jsdval->val);
        else
        {
            exceptionState = JS_SaveExceptionState(cx);
            jsdval->string = JS_ValueToString(cx, jsdval->val);
            JS_RestoreExceptionState(cx, exceptionState);
            if(jsdval->string)
            {
                if(!JS_AddNamedRoot(cx, &jsdval->string, "ValueString"))
                    jsdval->string = NULL;
            }
        }
    }
    return jsdval->string;
}

const char*
jsd_GetValueFunctionName(JSDContext* jsdc, JSDValue* jsdval)
{
    JSContext* cx = jsdc->dumbContext;
    JSFunction* fun;
    JSExceptionState* exceptionState;

    if(!jsdval->funName && jsd_IsValueFunction(jsdc, jsdval))
    {
        exceptionState = JS_SaveExceptionState(cx);
        fun = JS_ValueToFunction(cx, jsdval->val);
        JS_RestoreExceptionState(cx, exceptionState);
        if(!fun)
            return NULL;
        jsdval->funName = JS_GetFunctionName(fun);
    }
    return jsdval->funName;
}

/***************************************************************************/

JSDValue*
jsd_NewValue(JSDContext* jsdc, jsval val)
{
    JSDValue* jsdval;

    if(!(jsdval = (JSDValue*) calloc(1, sizeof(JSDValue))))
        return NULL;

    if(JSVAL_IS_GCTHING(val))
    {
        if(!JS_AddNamedRoot(jsdc->dumbContext, &jsdval->val, "JSDValue"))
        {
            free(jsdval);
            return NULL;
        }
    }
    jsdval->val  = val;
    jsdval->nref = 1;
    JS_INIT_CLIST(&jsdval->props);

    return jsdval;
}

void
jsd_DropValue(JSDContext* jsdc, JSDValue* jsdval)
{
    JS_ASSERT(jsdval->nref > 0);
    if(0 == --jsdval->nref)
    {
        jsd_RefreshValue(jsdc, jsdval);
        if(JSVAL_IS_GCTHING(jsdval->val))
            JS_RemoveRoot(jsdc->dumbContext, &jsdval->val);
        free(jsdval);
    }
}

jsval
jsd_GetValueWrappedJSVal(JSDContext* jsdc, JSDValue* jsdval)
{
    return jsdval->val;
}

static JSDProperty* _newProperty(JSDContext* jsdc, JSPropertyDesc* pd,
                                 uintN additionalFlags)
{
    JSDProperty* jsdprop;

    if(!(jsdprop = (JSDProperty*) calloc(1, sizeof(JSDProperty))))
        return NULL;

    JS_INIT_CLIST(&jsdprop->links);
    jsdprop->nref = 1;
    jsdprop->flags = pd->flags | additionalFlags;
    jsdprop->slot = pd->slot;

    if(!(jsdprop->name = jsd_NewValue(jsdc, pd->id)))
        goto new_prop_fail;

    if(!(jsdprop->val = jsd_NewValue(jsdc, pd->value)))
        goto new_prop_fail;

    if((jsdprop->flags & JSDPD_ALIAS) &&
       !(jsdprop->alias = jsd_NewValue(jsdc, pd->alias)))
        goto new_prop_fail;

    return jsdprop;
new_prop_fail:
    jsd_DropProperty(jsdc, jsdprop);
    return NULL;
}

static void _freeProps(JSDContext* jsdc, JSDValue* jsdval)
{
    JSDProperty* jsdprop;

    while(jsdprop = (JSDProperty*)jsdval->props.next,
          jsdprop != (JSDProperty*)&jsdval->props)
    {
        JS_REMOVE_AND_INIT_LINK(&jsdprop->links);
        jsd_DropProperty(jsdc, jsdprop);
    }
    JS_ASSERT(JS_CLIST_IS_EMPTY(&jsdval->props));
    CLEAR_BIT_FLAG(jsdval->flags, GOT_PROPS);
}

static JSBool _buildProps(JSDContext* jsdc, JSDValue* jsdval)
{
    JSContext* cx = jsdc->dumbContext;
    JSPropertyDescArray pda;
    uintN i;

    JS_ASSERT(JS_CLIST_IS_EMPTY(&jsdval->props));
    JS_ASSERT(!(CHECK_BIT_FLAG(jsdval->flags, GOT_PROPS)));
    JS_ASSERT(JSVAL_IS_OBJECT(jsdval->val));

    if(!JSVAL_IS_OBJECT(jsdval->val) || JSVAL_IS_NULL(jsdval->val))
        return JS_FALSE;

    if(!JS_GetPropertyDescArray(cx, JSVAL_TO_OBJECT(jsdval->val), &pda))
        return JS_FALSE;

    for(i = 0; i < pda.length; i++)
    {
        JSDProperty* prop = _newProperty(jsdc, &pda.array[i], 0);
        if(!prop)
        {
            _freeProps(jsdc, jsdval);
            break;
        }
        JS_APPEND_LINK(&prop->links, &jsdval->props);
    }
    JS_PutPropertyDescArray(cx, &pda);
    SET_BIT_FLAG(jsdval->flags, GOT_PROPS);
    return !JS_CLIST_IS_EMPTY(&jsdval->props);
}

#undef  DROP_CLEAR_VALUE
#define DROP_CLEAR_VALUE(jsdc, x) if(x){jsd_DropValue(jsdc,x); x = NULL;}

void
jsd_RefreshValue(JSDContext* jsdc, JSDValue* jsdval)
{
    JSContext* cx = jsdc->dumbContext;

    if(jsdval->string)
    {
        /* if the jsval is a string, then we didn't need to root the string */
        if(!JSVAL_IS_STRING(jsdval->val))
            JS_RemoveRoot(cx, &jsdval->string);
        jsdval->string = NULL;
    }

    jsdval->funName = NULL;
    jsdval->className = NULL;
    DROP_CLEAR_VALUE(jsdc, jsdval->proto);
    DROP_CLEAR_VALUE(jsdc, jsdval->parent);
    DROP_CLEAR_VALUE(jsdc, jsdval->ctor);
    _freeProps(jsdc, jsdval);
    jsdval->flags = 0;
}

/***************************************************************************/

uintN
jsd_GetCountOfProperties(JSDContext* jsdc, JSDValue* jsdval)
{
    JSDProperty* jsdprop;
    uintN count = 0;

    if(!(CHECK_BIT_FLAG(jsdval->flags, GOT_PROPS)))
        if(!_buildProps(jsdc, jsdval))
            return 0;

    for(jsdprop = (JSDProperty*)jsdval->props.next;
        jsdprop != (JSDProperty*)&jsdval->props;
        jsdprop = (JSDProperty*)jsdprop->links.next)
    {
        count++;
    }
    return count;
}

JSDProperty*
jsd_IterateProperties(JSDContext* jsdc, JSDValue* jsdval, JSDProperty **iterp)
{
    JSDProperty* jsdprop = *iterp;
    if(!(CHECK_BIT_FLAG(jsdval->flags, GOT_PROPS)))
    {
        JS_ASSERT(!jsdprop);
        if(!_buildProps(jsdc, jsdval))
            return NULL;
    }

    if(!jsdprop)
        jsdprop = (JSDProperty*)jsdval->props.next;
    if(jsdprop == (JSDProperty*)&jsdval->props)
        return NULL;
    *iterp = (JSDProperty*)jsdprop->links.next;

    JS_ASSERT(jsdprop);
    jsdprop->nref++;
    return jsdprop;
}

JSDProperty*
jsd_GetValueProperty(JSDContext* jsdc, JSDValue* jsdval, JSString* name)
{
    JSContext* cx = jsdc->dumbContext;
    JSDProperty* jsdprop;
    JSDProperty* iter = NULL;
    JSObject* obj;
    uintN  attrs = 0;
    JSBool found;
    JSPropertyDesc pd;
    const jschar * nameChars;
    size_t nameLen;
    jsval val;

    if(!jsd_IsValueObject(jsdc, jsdval))
        return NULL;

    /* If we already have the prop, then return it */
    while(NULL != (jsdprop = jsd_IterateProperties(jsdc, jsdval, &iter)))
    {
        JSString* propName = jsd_GetValueString(jsdc, jsdprop->name);
        if(propName && !JS_CompareStrings(propName, name))
            return jsdprop;
        JSD_DropProperty(jsdc, jsdprop);
    }
    /* Not found in property list, look it up explicitly */

    if(!(obj = JSVAL_TO_OBJECT(jsdval->val)))
        return NULL;

    nameChars = JS_GetStringChars(name);
    nameLen   = JS_GetStringLength(name);

    JS_GetUCPropertyAttributes(cx, obj, nameChars, nameLen, &attrs, &found);
    if (!found)
        return NULL;

    JS_ClearPendingException(cx);

    if(!JS_GetUCProperty(cx, obj, nameChars, nameLen, &val))
    {
        if (JS_IsExceptionPending(cx))
        {
            if (!JS_GetPendingException(cx, &pd.value))
                return NULL;
            pd.flags = JSPD_EXCEPTION;
        }
        else
        {
            pd.flags = JSPD_ERROR;
            pd.value = JSVAL_VOID;
        }
    }
    else
    {
        pd.value = val;
    }

    pd.id = STRING_TO_JSVAL(name);
    pd.alias = pd.slot = pd.spare = 0;
    pd.flags |= (attrs & JSPROP_ENUMERATE) ? JSPD_ENUMERATE : 0
        | (attrs & JSPROP_READONLY)  ? JSPD_READONLY  : 0
        | (attrs & JSPROP_PERMANENT) ? JSPD_PERMANENT : 0;

    return _newProperty(jsdc, &pd, JSDPD_HINTED);
}


JSDValue*
jsd_GetValuePrototype(JSDContext* jsdc, JSDValue* jsdval)
{
    if(!(CHECK_BIT_FLAG(jsdval->flags, GOT_PROTO)))
    {
        JSObject* obj;
        JSObject* proto;
        JS_ASSERT(!jsdval->proto);
        SET_BIT_FLAG(jsdval->flags, GOT_PROTO);
        if(!JSVAL_IS_OBJECT(jsdval->val))
            return NULL;
        if(!(obj = JSVAL_TO_OBJECT(jsdval->val)))
            return NULL;
        if(!(proto = JS_GetPrototype(jsdc->dumbContext, obj)))
            return NULL;
        jsdval->proto = jsd_NewValue(jsdc, OBJECT_TO_JSVAL(proto));
    }
    if(jsdval->proto)
        jsdval->proto->nref++;
    return jsdval->proto;
}

JSDValue*
jsd_GetValueParent(JSDContext* jsdc, JSDValue* jsdval)
{
    if(!(CHECK_BIT_FLAG(jsdval->flags, GOT_PARENT)))
    {
        JSObject* obj;
        JSObject* parent;
        JS_ASSERT(!jsdval->parent);
        SET_BIT_FLAG(jsdval->flags, GOT_PARENT);
        if(!JSVAL_IS_OBJECT(jsdval->val))
            return NULL;
        if(!(obj = JSVAL_TO_OBJECT(jsdval->val)))
            return NULL;
        if(!(parent = JS_GetParent(jsdc->dumbContext,obj)))
            return NULL;
        jsdval->parent = jsd_NewValue(jsdc, OBJECT_TO_JSVAL(parent));
    }
    if(jsdval->parent)
        jsdval->parent->nref++;
    return jsdval->parent;
}

JSDValue*
jsd_GetValueConstructor(JSDContext* jsdc, JSDValue* jsdval)
{
    if(!(CHECK_BIT_FLAG(jsdval->flags, GOT_CTOR)))
    {
        JSObject* obj;
        JSObject* proto;
        JSObject* ctor;
        JS_ASSERT(!jsdval->ctor);
        SET_BIT_FLAG(jsdval->flags, GOT_CTOR);
        if(!JSVAL_IS_OBJECT(jsdval->val))
            return NULL;
        if(!(obj = JSVAL_TO_OBJECT(jsdval->val)))
            return NULL;
        if(!(proto = JS_GetPrototype(jsdc->dumbContext,obj)))
            return NULL;
        if(!(ctor = JS_GetConstructor(jsdc->dumbContext,proto)))
            return NULL;
        jsdval->ctor = jsd_NewValue(jsdc, OBJECT_TO_JSVAL(ctor));
    }
    if(jsdval->ctor)
        jsdval->ctor->nref++;
    return jsdval->ctor;
}

const char*
jsd_GetValueClassName(JSDContext* jsdc, JSDValue* jsdval)
{
    jsval val = jsdval->val;
    if(!jsdval->className && JSVAL_IS_OBJECT(val))
    {
        JSObject* obj;
        if(!(obj = JSVAL_TO_OBJECT(val)))
            return NULL;
        if(JS_GET_CLASS(jsdc->dumbContext, obj))
            jsdval->className = JS_GET_CLASS(jsdc->dumbContext, obj)->name;
    }
    return jsdval->className;
}

/***************************************************************************/
/***************************************************************************/

JSDValue*
jsd_GetPropertyName(JSDContext* jsdc, JSDProperty* jsdprop)
{
    jsdprop->name->nref++;
    return jsdprop->name;
}

JSDValue*
jsd_GetPropertyValue(JSDContext* jsdc, JSDProperty* jsdprop)
{
    jsdprop->val->nref++;
    return jsdprop->val;
}

JSDValue*
jsd_GetPropertyAlias(JSDContext* jsdc, JSDProperty* jsdprop)
{
    if(jsdprop->alias)
        jsdprop->alias->nref++;
    return jsdprop->alias;
}

uintN
jsd_GetPropertyFlags(JSDContext* jsdc, JSDProperty* jsdprop)
{
    return jsdprop->flags;
}

uintN
jsd_GetPropertyVarArgSlot(JSDContext* jsdc, JSDProperty* jsdprop)
{
    return jsdprop->slot;
}

void
jsd_DropProperty(JSDContext* jsdc, JSDProperty* jsdprop)
{
    JS_ASSERT(jsdprop->nref > 0);
    if(0 == --jsdprop->nref)
    {
        JS_ASSERT(JS_CLIST_IS_EMPTY(&jsdprop->links));
        DROP_CLEAR_VALUE(jsdc, jsdprop->val);
        DROP_CLEAR_VALUE(jsdc, jsdprop->name);
        DROP_CLEAR_VALUE(jsdc, jsdprop->alias);
        free(jsdprop);
    }
}
