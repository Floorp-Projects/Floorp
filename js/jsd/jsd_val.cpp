/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JavaScript Debugging support - Value and Property support
 */

#include "jsd.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "jswrapper.h"
#include "nsCxPusher.h"

using mozilla::AutoSafeJSContext;

#ifdef DEBUG
void JSD_ASSERT_VALID_VALUE(JSDValue* jsdval)
{
    MOZ_ASSERT(jsdval);
    MOZ_ASSERT(jsdval->nref > 0);
    if(!JS_CLIST_IS_EMPTY(&jsdval->props))
    {
        MOZ_ASSERT(CHECK_BIT_FLAG(jsdval->flags, GOT_PROPS));
        MOZ_ASSERT(!jsdval->val.isPrimitive());
    }

    if(jsdval->proto)
    {
        MOZ_ASSERT(CHECK_BIT_FLAG(jsdval->flags, GOT_PROTO));
        MOZ_ASSERT(jsdval->proto->nref > 0);
    }
    if(jsdval->parent)
    {
        MOZ_ASSERT(CHECK_BIT_FLAG(jsdval->flags, GOT_PARENT));
        MOZ_ASSERT(jsdval->parent->nref > 0);
    }
    if(jsdval->ctor)
    {
        MOZ_ASSERT(CHECK_BIT_FLAG(jsdval->flags, GOT_CTOR));
        MOZ_ASSERT(jsdval->ctor->nref > 0);
    }
}

void JSD_ASSERT_VALID_PROPERTY(JSDProperty* jsdprop)
{
    MOZ_ASSERT(jsdprop);
    MOZ_ASSERT(jsdprop->name);
    MOZ_ASSERT(jsdprop->name->nref > 0);
    MOZ_ASSERT(jsdprop->val);
    MOZ_ASSERT(jsdprop->val->nref > 0);
    if(jsdprop->alias)
        MOZ_ASSERT(jsdprop->alias->nref > 0);
}
#endif


bool
jsd_IsValueObject(JSDContext* jsdc, JSDValue* jsdval)
{
    return !jsdval->val.isPrimitive() || jsdval->val.isNull();
}

bool
jsd_IsValueNumber(JSDContext* jsdc, JSDValue* jsdval)
{
    return jsdval->val.isNumber();
}

bool
jsd_IsValueInt(JSDContext* jsdc, JSDValue* jsdval)
{
    return jsdval->val.isInt32();
}

bool
jsd_IsValueDouble(JSDContext* jsdc, JSDValue* jsdval)
{
    return jsdval->val.isDouble();
}

bool
jsd_IsValueString(JSDContext* jsdc, JSDValue* jsdval)
{
    return jsdval->val.isString();
}

bool
jsd_IsValueBoolean(JSDContext* jsdc, JSDValue* jsdval)
{
    return jsdval->val.isBoolean();
}

bool
jsd_IsValueNull(JSDContext* jsdc, JSDValue* jsdval)
{
    return jsdval->val.isNull();
}

bool
jsd_IsValueVoid(JSDContext* jsdc, JSDValue* jsdval)
{
    return jsdval->val.isUndefined();
}

bool
jsd_IsValuePrimitive(JSDContext* jsdc, JSDValue* jsdval)
{
    return jsdval->val.isPrimitive();
}

bool
jsd_IsValueFunction(JSDContext* jsdc, JSDValue* jsdval)
{
    AutoSafeJSContext cx; // NB: Actually unused.
    return !jsdval->val.isPrimitive() &&
           JS_ObjectIsCallable(cx, jsdval->val.toObjectOrNull());
}

bool
jsd_IsValueNative(JSDContext* jsdc, JSDValue* jsdval)
{
    AutoSafeJSContext cx;
    JS::RootedFunction fun(cx);

    if(jsd_IsValueFunction(jsdc, jsdval))
    {
        JSAutoCompartment ac(cx, jsdval->val.toObjectOrNull());
        AutoSaveExceptionState as(cx);
        bool ok = false;
        fun = JSD_GetValueFunction(jsdc, jsdval);
        if(fun)
            ok = JS_GetFunctionScript(cx, fun) ? false : true;
        MOZ_ASSERT(fun);
        return ok;
    }
    return !jsdval->val.isPrimitive();
}

/***************************************************************************/

bool
jsd_GetValueBoolean(JSDContext* jsdc, JSDValue* jsdval)
{
    jsval val = jsdval->val;
    if(!val.isBoolean())
        return false;
    return val.toBoolean();
}

int32_t
jsd_GetValueInt(JSDContext* jsdc, JSDValue* jsdval)
{
    jsval val = jsdval->val;
    if(!val.isInt32())
        return 0;
    return val.toInt32();
}

double
jsd_GetValueDouble(JSDContext* jsdc, JSDValue* jsdval)
{
    if(!jsdval->val.isDouble())
        return 0;
    return jsdval->val.toDouble();
}

JSString*
jsd_GetValueString(JSDContext* jsdc, JSDValue* jsdval)
{
    AutoSafeJSContext cx;
    JS::RootedValue stringval(cx);
    JS::RootedString string(cx);
    JS::RootedObject scopeObj(cx);

    if(jsdval->string)
        return jsdval->string;

    /* Reuse the string without copying or re-rooting it */
    if(jsdval->val.isString()) {
        jsdval->string = jsdval->val.toString();
        return jsdval->string;
    }

    /* Objects call JS_ValueToString in their own compartment. */
    scopeObj = !jsdval->val.isPrimitive() ? jsdval->val.toObjectOrNull() : jsdc->glob;
    {
        JSAutoCompartment ac(cx, scopeObj);
        AutoSaveExceptionState as(cx);
        JS::RootedValue v(cx, jsdval->val);
        string = JS::ToString(cx, v);
    }

    JSAutoCompartment ac2(cx, jsdc->glob);
    if(string) {
        stringval = STRING_TO_JSVAL(string);
    }
    if(!string || !JS_WrapValue(cx, &stringval)) {
        return nullptr;
    }

    jsdval->string = stringval.toString();
    if(!JS::AddNamedStringRoot(cx, &jsdval->string, "ValueString"))
        jsdval->string = nullptr;

    return jsdval->string;
}

JSString*
jsd_GetValueFunctionId(JSDContext* jsdc, JSDValue* jsdval)
{
    AutoSafeJSContext cx;
    JS::RootedFunction fun(cx);

    if(!jsdval->funName && jsd_IsValueFunction(jsdc, jsdval))
    {
        JSAutoCompartment ac(cx, jsdval->val.toObjectOrNull());
        AutoSaveExceptionState as(cx);
        fun = JSD_GetValueFunction(jsdc, jsdval);
        if(!fun)
            return nullptr;
        jsdval->funName = JS_GetFunctionId(fun);

        /* For compatibility we return "anonymous", not an empty string here. */
        if (!jsdval->funName)
            jsdval->funName = JS_GetAnonymousString(jsdc->jsrt);
    }
    return jsdval->funName;
}

/***************************************************************************/

/*
 * Create a new JSD value referring to a jsval. Copy string values into the
 * JSD compartment. Leave all other GCTHINGs in their native compartments
 * and access them through cross-compartment calls.
 */
JSDValue*
jsd_NewValue(JSDContext* jsdc, jsval value)
{
    JS::RootedValue val(jsdc->jsrt, value);
    AutoSafeJSContext cx;
    JSDValue* jsdval;

    if(!(jsdval = (JSDValue*) calloc(1, sizeof(JSDValue))))
        return nullptr;

    if(val.isGCThing())
    {
        bool ok;
        JSAutoCompartment ac(cx, jsdc->glob);

        ok = JS::AddNamedValueRoot(cx, &jsdval->val, "JSDValue");
        if(ok && val.isString()) {
            if(!JS_WrapValue(cx, &val)) {
                ok = false;
            }
        }

        if(!ok)
        {
            free(jsdval);
            return nullptr;
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
    MOZ_ASSERT(jsdval->nref > 0);
    if(0 == --jsdval->nref)
    {
        jsd_RefreshValue(jsdc, jsdval);
        if(jsdval->val.isGCThing())
        {
            AutoSafeJSContext cx;
            JSAutoCompartment ac(cx, jsdc->glob);
            JS::RemoveValueRoot(cx, &jsdval->val);
        }
        free(jsdval);
    }
}

jsval
jsd_GetValueWrappedJSVal(JSDContext* jsdc, JSDValue* jsdval)
{
    AutoSafeJSContext cx;
    JS::RootedValue val(cx, jsdval->val);
    if (!val.isPrimitive()) {
        JS::RootedObject obj(cx, &val.toObject());
        JSAutoCompartment ac(cx, obj);
        obj = JS_ObjectToOuterObject(cx, obj);
        if (!obj)
        {
            JS_ClearPendingException(cx);
            val = JSVAL_NULL;
        }
        else
            val = JS::ObjectValue(*obj);
    }

    return val;
}

static JSDProperty* _newProperty(JSDContext* jsdc, JS::HandleValue propId,
                                 JS::HandleValue propValue, JS::HandleValue propAlias,
                                 uint8_t propFlags, unsigned additionalFlags)
{
    JSDProperty* jsdprop;

    if(!(jsdprop = (JSDProperty*) calloc(1, sizeof(JSDProperty))))
        return nullptr;

    JS_INIT_CLIST(&jsdprop->links);
    jsdprop->nref = 1;
    jsdprop->flags = propFlags | additionalFlags;

    if(!(jsdprop->name = jsd_NewValue(jsdc, propId)))
        goto new_prop_fail;

    if(!(jsdprop->val = jsd_NewValue(jsdc, propValue)))
        goto new_prop_fail;

    if((jsdprop->flags & JSDPD_ALIAS) &&
       !(jsdprop->alias = jsd_NewValue(jsdc, propAlias)))
        goto new_prop_fail;

    return jsdprop;
new_prop_fail:
    jsd_DropProperty(jsdc, jsdprop);
    return nullptr;
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
    MOZ_ASSERT(JS_CLIST_IS_EMPTY(&jsdval->props));
    CLEAR_BIT_FLAG(jsdval->flags, GOT_PROPS);
}

static bool _buildProps(JSDContext* jsdc, JSDValue* jsdval)
{
    AutoSafeJSContext cx;
    JS::RootedObject obj(cx);
    JSPropertyDescArray pda;
    unsigned i;

    MOZ_ASSERT(JS_CLIST_IS_EMPTY(&jsdval->props));
    MOZ_ASSERT(!(CHECK_BIT_FLAG(jsdval->flags, GOT_PROPS)));
    MOZ_ASSERT(!jsdval->val.isPrimitive());

    if(jsdval->val.isPrimitive())
        return false;

    obj = jsdval->val.toObjectOrNull();

    JSAutoCompartment ac(cx, obj);

    if(!JS_GetPropertyDescArray(cx, obj, &pda))
    {
        return false;
    }

    JS::RootedValue propId(cx);
    JS::RootedValue propValue(cx);
    JS::RootedValue propAlias(cx);
    uint8_t propFlags;
    for(i = 0; i < pda.length; i++)
    {
        propId = pda.array[i].id;
        propValue = pda.array[i].value;
        propAlias = pda.array[i].alias;
        propFlags = pda.array[i].flags;
        JSDProperty* prop = _newProperty(jsdc, propId, propValue, propAlias, propFlags, 0);
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
#define DROP_CLEAR_VALUE(jsdc, x) if(x){jsd_DropValue(jsdc,x); x = nullptr;}

void
jsd_RefreshValue(JSDContext* jsdc, JSDValue* jsdval)
{
    AutoSafeJSContext cx;
    if(jsdval->string)
    {
        /* if the jsval is a string, then we didn't need to root the string */
        if(!jsdval->val.isString())
        {
            JSAutoCompartment ac(cx, jsdc->glob);
            JS::RemoveStringRoot(cx, &jsdval->string);
        }
        jsdval->string = nullptr;
    }

    jsdval->funName = nullptr;
    jsdval->className = nullptr;
    DROP_CLEAR_VALUE(jsdc, jsdval->proto);
    DROP_CLEAR_VALUE(jsdc, jsdval->parent);
    DROP_CLEAR_VALUE(jsdc, jsdval->ctor);
    _freeProps(jsdc, jsdval);
    jsdval->flags = 0;
}

/***************************************************************************/

unsigned
jsd_GetCountOfProperties(JSDContext* jsdc, JSDValue* jsdval)
{
    JSDProperty* jsdprop;
    unsigned count = 0;

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
        MOZ_ASSERT(!jsdprop);
        if(!_buildProps(jsdc, jsdval))
            return nullptr;
    }

    if(!jsdprop)
        jsdprop = (JSDProperty*)jsdval->props.next;
    if(jsdprop == (JSDProperty*)&jsdval->props)
        return nullptr;
    *iterp = (JSDProperty*)jsdprop->links.next;

    MOZ_ASSERT(jsdprop);
    jsdprop->nref++;
    return jsdprop;
}

JSDProperty*
jsd_GetValueProperty(JSDContext* jsdc, JSDValue* jsdval, JSString* nameStr)
{
    JS::RootedString name(jsdc->jsrt, nameStr);
    AutoSafeJSContext cx;
    JSAutoCompartment acBase(cx, jsdc->glob);
    JSDProperty* jsdprop;
    JSDProperty* iter = nullptr;
    JS::RootedObject obj(cx);
    JS::RootedValue val(cx), nameval(cx);
    JS::RootedId nameid(cx);
    JS::RootedValue propId(cx);
    JS::RootedValue propValue(cx);
    JS::RootedValue propAlias(cx);
    uint8_t propFlags;

    if(!jsd_IsValueObject(jsdc, jsdval))
        return nullptr;

    /* If we already have the prop, then return it */
    while(nullptr != (jsdprop = jsd_IterateProperties(jsdc, jsdval, &iter)))
    {
        JSString* propName = jsd_GetValueString(jsdc, jsdprop->name);
        if(propName) {
            int result;
            if (JS_CompareStrings(cx, propName, name, &result) && !result)
                return jsdprop;
        }
        JSD_DropProperty(jsdc, jsdprop);
    }
    /* Not found in property list, look it up explicitly */

    nameval = STRING_TO_JSVAL(name);
    if(!JS_ValueToId(cx, nameval, &nameid))
        return nullptr;

    if(!(obj = jsdval->val.toObjectOrNull()))
        return nullptr;

    JS::Rooted<JSPropertyDescriptor> desc(cx);
    {
        JSAutoCompartment ac(cx, obj);
        JS::RootedId id(cx, nameid);

        if(!JS_GetOwnPropertyDescriptorById(cx, obj, id, &desc))
            return nullptr;
        if(!desc.object())
            return nullptr;

        JS_ClearPendingException(cx);

        if(!JS_GetPropertyById(cx, obj, id, &val))
        {
            if (JS_IsExceptionPending(cx))
            {
                if (!JS_GetPendingException(cx, &propValue))
                {
                    return nullptr;
                }
                propFlags = JSPD_EXCEPTION;
            }
            else
            {
                propFlags = JSPD_ERROR;
                propValue = JSVAL_VOID;
            }
        }
        else
        {
            propValue = val;
        }
    }

    if (!JS_IdToValue(cx, nameid, &propId))
        return nullptr;

    propAlias = JSVAL_NULL;
    propFlags |= desc.isEnumerable() ? JSPD_ENUMERATE : 0
        | desc.isReadonly() ? JSPD_READONLY  : 0
        | desc.isPermanent() ? JSPD_PERMANENT : 0;

    return _newProperty(jsdc, propId, propValue, propAlias, propFlags, JSDPD_HINTED);
}

/*
 * Retrieve a JSFunction* from a JSDValue*. This differs from
 * JS_ValueToFunction by fully unwrapping the object first.
 */
JSFunction*
jsd_GetValueFunction(JSDContext* jsdc, JSDValue* jsdval)
{
    AutoSafeJSContext cx;

    JS::RootedObject obj(cx);
    JS::RootedFunction fun(cx);

    if (jsdval->val.isPrimitive())
        return nullptr;

    obj = js::UncheckedUnwrap(jsdval->val.toObjectOrNull());
    JSAutoCompartment ac(cx, obj);
    JS::RootedValue funval(cx, JS::ObjectValue(*obj));
    fun = JS_ValueToFunction(cx, funval);

    return fun;
}

JSDValue*
jsd_GetValuePrototype(JSDContext* jsdc, JSDValue* jsdval)
{
    AutoSafeJSContext cx;
    if(!(CHECK_BIT_FLAG(jsdval->flags, GOT_PROTO)))
    {
        JS::RootedObject obj(cx);
        JS::RootedObject proto(cx);
        MOZ_ASSERT(!jsdval->proto);
        SET_BIT_FLAG(jsdval->flags, GOT_PROTO);
        if(jsdval->val.isPrimitive())
            return nullptr;
        obj = jsdval->val.toObjectOrNull();
        if(!JS_GetPrototype(cx, obj, &proto))
            return nullptr;
        if(!proto)
            return nullptr;
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
        AutoSafeJSContext cx;
        JS::RootedObject obj(cx);
        JS::RootedObject parent(cx);
        MOZ_ASSERT(!jsdval->parent);
        SET_BIT_FLAG(jsdval->flags, GOT_PARENT);
        if(jsdval->val.isPrimitive())
            return nullptr;
        obj = jsdval->val.toObjectOrNull();
        {
            JSAutoCompartment ac(cx, obj);
            parent = JS_GetParentOrScopeChain(cx, obj);
        }
        if(!parent)
            return nullptr;
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
        AutoSafeJSContext cx;
        JS::RootedObject obj(cx);
        JS::RootedObject proto(cx);
        JS::RootedObject ctor(cx);
        MOZ_ASSERT(!jsdval->ctor);
        SET_BIT_FLAG(jsdval->flags, GOT_CTOR);
        if(jsdval->val.isPrimitive())
            return nullptr;
        obj = jsdval->val.toObjectOrNull();
        if(!JS_GetPrototype(cx, obj, &proto))
            return nullptr;
        if(!proto)
            return nullptr;
        {
            JSAutoCompartment ac(cx, obj);
            ctor = JS_GetConstructor(cx, proto);
        }
        if(!ctor)
            return nullptr;
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
    if(!jsdval->className && !val.isPrimitive())
    {
        JS::RootedObject obj(jsdc->jsrt, val.toObjectOrNull());
        AutoSafeJSContext cx;
        JSAutoCompartment ac(cx, obj);
        jsdval->className = JS_GetDebugClassName(obj);
    }
    return jsdval->className;
}

JSDScript*
jsd_GetScriptForValue(JSDContext* jsdc, JSDValue* jsdval)
{
    AutoSafeJSContext cx;
    JS::RootedValue val(cx, jsdval->val);
    JS::RootedScript script(cx);
    JSDScript* jsdscript;

    if (!jsd_IsValueFunction(jsdc, jsdval))
        return nullptr;

    {
        JSAutoCompartment ac(cx, val.toObjectOrNull());
        AutoSaveExceptionState as(cx);
        JS::RootedFunction fun(cx, JSD_GetValueFunction(jsdc, jsdval));
        if (fun)
            script = JS_GetFunctionScript(cx, fun);
    }

    if (!script)
        return nullptr;

    JSD_LOCK_SCRIPTS(jsdc);
    jsdscript = jsd_FindJSDScript(jsdc, script);
    JSD_UNLOCK_SCRIPTS(jsdc);
    return jsdscript;
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

unsigned
jsd_GetPropertyFlags(JSDContext* jsdc, JSDProperty* jsdprop)
{
    return jsdprop->flags;
}

void
jsd_DropProperty(JSDContext* jsdc, JSDProperty* jsdprop)
{
    MOZ_ASSERT(jsdprop->nref > 0);
    if(0 == --jsdprop->nref)
    {
        MOZ_ASSERT(JS_CLIST_IS_EMPTY(&jsdprop->links));
        DROP_CLEAR_VALUE(jsdc, jsdprop->val);
        DROP_CLEAR_VALUE(jsdc, jsdprop->name);
        DROP_CLEAR_VALUE(jsdc, jsdprop->alias);
        free(jsdprop);
    }
}
