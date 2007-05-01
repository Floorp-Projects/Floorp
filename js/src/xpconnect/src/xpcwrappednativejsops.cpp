/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/* JavaScript JSClasses and JSOps for our Wrapped Native JS Objects. */

#include "xpcprivate.h"
#include "XPCNativeWrapper.h"

/***************************************************************************/

// All of the exceptions thrown into JS from this file go through here.
// That makes this a nice place to set a breakpoint.

static JSBool Throw(uintN errNum, JSContext* cx)
{
    XPCThrower::Throw(errNum, cx);
    return JS_FALSE;
}

// Handy macro used in many callback stub below.

#define THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper)                         \
    PR_BEGIN_MACRO                                                           \
    if(!wrapper)                                                             \
        return Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);                   \
    if(!wrapper->IsValid())                                                  \
        return Throw(NS_ERROR_XPC_HAS_BEEN_SHUTDOWN, cx);                    \
    PR_END_MACRO

// We rely on the engine only giving us jsval ids that are actually the
// self-same jsvals that are in the atom table (that is, if the id represents
// a string). So, we assert by converting the jsval to an id and then back
// to a jsval and comparing pointers. If the engine ever breaks this promise
// then we will scream.
#ifdef DEBUG
#define CHECK_IDVAL(cx, idval)                                               \
    PR_BEGIN_MACRO                                                           \
    if(JSVAL_IS_STRING(idval))                                               \
    {                                                                        \
        jsid d_id;                                                           \
        jsval d_val;                                                         \
        NS_ASSERTION(JS_ValueToId(cx, idval, &d_id), "JS_ValueToId failed!");\
        NS_ASSERTION(JS_IdToValue(cx, d_id, &d_val), "JS_IdToValue failed!");\
        NS_ASSERTION(d_val == idval, "id differs from id in atom table!");   \
    }                                                                        \
    PR_END_MACRO
#else
#define CHECK_IDVAL(cx, idval) ((void)0)
#endif

/***************************************************************************/

static JSBool
ToStringGuts(XPCCallContext& ccx)
{
    char* sz;
    XPCWrappedNative* wrapper = ccx.GetWrapper();

    if(wrapper)
        sz = wrapper->ToString(ccx, ccx.GetTearOff());
    else
        sz = JS_smprintf("[xpconnect wrapped native prototype]");

    if(!sz)
    {
        JS_ReportOutOfMemory(ccx);
        return JS_FALSE;
    }

    JSString* str = JS_NewString(ccx, sz, strlen(sz));
    if(!str)
    {
        JS_smprintf_free(sz);
        // JS_ReportOutOfMemory already reported by failed JS_NewString
        return JS_FALSE;
    }

    ccx.SetRetVal(STRING_TO_JSVAL(str));
    return JS_TRUE;
}

/***************************************************************************/

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_Shared_ToString(JSContext *cx, JSObject *obj,
                       uintN argc, jsval *argv, jsval *vp)
{
    XPCCallContext ccx(JS_CALLER, cx, obj);
    ccx.SetName(ccx.GetRuntime()->GetStringJSVal(XPCJSRuntime::IDX_TO_STRING));
    ccx.SetArgsAndResultPtr(argc, argv, vp);
    return ToStringGuts(ccx);
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_Shared_ToSource(JSContext *cx, JSObject *obj,
                       uintN argc, jsval *argv, jsval *vp)
{
    static const char empty[] = "{}";
    *vp = STRING_TO_JSVAL(JS_NewStringCopyN(cx, empty, sizeof(empty)-1));
    return JS_TRUE;
}

/***************************************************************************/

// A "double wrapped object" is a user JSObject that has been wrapped as a
// wrappedJS in order to be used by native code and then re-wrapped by a
// wrappedNative wrapper to be used by JS code. One might think of it as:
//    wrappedNative(wrappedJS(underlying_JSObject))
// This is done (as opposed to just unwrapping the wrapped JS and automatically
// returning the underlying JSObject) so that JS callers will see what looks
// Like any other xpcom object - and be limited to use its interfaces.
//
// See the comment preceeding nsIXPCWrappedJSObjectGetter in nsIXPConnect.idl.

static JSObject*
GetDoubleWrappedJSObject(XPCCallContext& ccx, XPCWrappedNative* wrapper)
{
    JSObject* obj = nsnull;
    nsCOMPtr<nsIXPConnectWrappedJS>
        underware = do_QueryInterface(wrapper->GetIdentityObject());
    if(underware)
    {
        JSObject* mainObj = nsnull;
        if(NS_SUCCEEDED(underware->GetJSObject(&mainObj)) && mainObj)
        {
            jsid id = ccx.GetRuntime()->
                    GetStringID(XPCJSRuntime::IDX_WRAPPED_JSOBJECT);

            jsval val;
            if(OBJ_GET_PROPERTY(ccx, mainObj, id,
                                &val) && !JSVAL_IS_PRIMITIVE(val))
            {
                obj = JSVAL_TO_OBJECT(val);
            }
        }
    }
    return obj;
}

// This is the getter native function we use to handle 'wrappedJSObject' for
// double wrapped JSObjects.

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_DoubleWrappedGetter(JSContext *cx, JSObject *obj,
                           uintN argc, jsval *argv, jsval *vp)
{
    XPCCallContext ccx(JS_CALLER, cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    NS_ASSERTION(JS_TypeOfValue(cx, argv[-2]) == JSTYPE_FUNCTION, "bad function");

    JSObject* realObject = GetDoubleWrappedJSObject(ccx, wrapper);
    if(!realObject)
    {
        // This is pretty unexpected at this point. The object originally
        // responded to this get property call and now gives no object.
        // XXX Should this throw something at the caller?
        *vp = JSVAL_NULL;
        return JS_TRUE;
    }

    // It is a double wrapped object. Figure out if the caller
    // is allowed to see it.

    nsIXPCSecurityManager* sm;
    XPCContext* xpcc = ccx.GetXPCContext();

    sm = xpcc->GetAppropriateSecurityManager(
                    nsIXPCSecurityManager::HOOK_GET_PROPERTY);
    if(sm)
    {
        AutoMarkingNativeInterfacePtr iface(ccx);
        iface = XPCNativeInterface::
                    GetNewOrUsed(ccx, &NS_GET_IID(nsIXPCWrappedJSObjectGetter));

        if(iface)
        {
            jsval idval = ccx.GetRuntime()->
                        GetStringJSVal(XPCJSRuntime::IDX_WRAPPED_JSOBJECT);

            ccx.SetCallInfo(iface, iface->GetMemberAt(1), JS_FALSE);
            if(NS_FAILED(sm->
                    CanAccess(nsIXPCSecurityManager::ACCESS_GET_PROPERTY,
                              &ccx, ccx,
                              ccx.GetFlattenedJSObject(),
                              wrapper->GetIdentityObject(),
                              wrapper->GetClassInfo(), idval,
                              wrapper->GetSecurityInfoAddr())))
            {
                // The SecurityManager should have set an exception.
                return JS_FALSE;
            }
        }
    }
    *vp = OBJECT_TO_JSVAL(realObject);
    return JS_TRUE;
}

/***************************************************************************/

// This is our shared function to define properties on our JSObjects.

/*
 * NOTE:
 * We *never* set the tearoff names (e.g. nsIFoo) as JS_ENUMERATE.
 * We *never* set toString or toSource as JS_ENUMERATE.
 */

static JSBool
DefinePropertyIfFound(XPCCallContext& ccx,
                      JSObject *obj, jsval idval,
                      XPCNativeSet* set,
                      XPCNativeInterface* iface,
                      XPCNativeMember* member,
                      XPCWrappedNativeScope* scope,
                      JSBool reflectToStringAndToSource,
                      XPCWrappedNative* wrapperToReflectInterfaceNames,
                      XPCWrappedNative* wrapperToReflectDoubleWrap,
                      XPCNativeScriptableInfo* scriptableInfo,
                      uintN propFlags,
                      JSBool* resolved)
{
    XPCJSRuntime* rt = ccx.GetRuntime();
    JSBool found;
    const char* name;
    jsid id;

    if(set)
    {
        if(iface)
            found = JS_TRUE;
        else
            found = set->FindMember(idval, &member, &iface);
    }
    else
        found = (nsnull != (member = iface->FindMember(idval)));

    if(!found)
    {
        HANDLE_POSSIBLE_NAME_CASE_ERROR(ccx, set, iface, idval);

        if(reflectToStringAndToSource)
        {
            JSNative call;

            if(idval == rt->GetStringJSVal(XPCJSRuntime::IDX_TO_STRING))
            {
                call = XPC_WN_Shared_ToString;
                name = rt->GetStringName(XPCJSRuntime::IDX_TO_STRING);
                id   = rt->GetStringID(XPCJSRuntime::IDX_TO_STRING);
            }
            else if(idval == rt->GetStringJSVal(XPCJSRuntime::IDX_TO_SOURCE))
            {
                call = XPC_WN_Shared_ToSource;
                name = rt->GetStringName(XPCJSRuntime::IDX_TO_SOURCE);
                id   = rt->GetStringID(XPCJSRuntime::IDX_TO_SOURCE);
            }

            else
                call = nsnull;

            if(call)
            {
                JSFunction* fun = JS_NewFunction(ccx, call, 0, 0, obj, name);
                if(!fun)
                {
                    JS_ReportOutOfMemory(ccx);
                    return JS_FALSE;
                }

                AutoResolveName arn(ccx, idval);
                if(resolved)
                    *resolved = JS_TRUE;
                return OBJ_DEFINE_PROPERTY(ccx, obj, id,
                                           OBJECT_TO_JSVAL(JS_GetFunctionObject(fun)),
                                           nsnull, nsnull,
                                           propFlags & ~JSPROP_ENUMERATE,
                                           nsnull);
            }
        }
        // This *might* be a tearoff name that is not yet part of our
        // set. Let's lookup the name and see if it is the name of an
        // interface. Then we'll see if the object actually *does* this
        // interface and add a tearoff as necessary.

        if(wrapperToReflectInterfaceNames)
        {
            AutoMarkingNativeInterfacePtr iface2(ccx);
            XPCWrappedNativeTearOff* to;
            JSObject* jso;

            if(JSVAL_IS_STRING(idval) &&
               nsnull != (name = JS_GetStringBytes(JSVAL_TO_STRING(idval))) &&
               (iface2 = XPCNativeInterface::GetNewOrUsed(ccx, name), iface2) &&
               nsnull != (to = wrapperToReflectInterfaceNames->
                                    FindTearOff(ccx, iface2, JS_TRUE)) &&
               nsnull != (jso = to->GetJSObject()))

            {
                AutoResolveName arn(ccx, idval);
                if(resolved)
                    *resolved = JS_TRUE;
                return JS_ValueToId(ccx, idval, &id) &&
                       OBJ_DEFINE_PROPERTY(ccx, obj, id, OBJECT_TO_JSVAL(jso),
                                           nsnull, nsnull,
                                           propFlags & ~JSPROP_ENUMERATE,
                                           nsnull);
            }
        }

        // This *might* be a double wrapped JSObject
        if(wrapperToReflectDoubleWrap &&
           idval == rt->GetStringJSVal(XPCJSRuntime::IDX_WRAPPED_JSOBJECT) &&
           GetDoubleWrappedJSObject(ccx, wrapperToReflectDoubleWrap))
        {
            // We build and add a getter function.
            // A security check is done on a per-get basis.

            JSFunction* fun;

            id = rt->GetStringID(XPCJSRuntime::IDX_WRAPPED_JSOBJECT);
            name = rt->GetStringName(XPCJSRuntime::IDX_WRAPPED_JSOBJECT);

            fun = JS_NewFunction(ccx, XPC_WN_DoubleWrappedGetter,
                                 0, JSFUN_GETTER, obj, name);

            if(!fun)
                return JS_FALSE;

            JSObject* funobj = JS_GetFunctionObject(fun);
            if(!funobj)
                return JS_FALSE;

            propFlags |= JSPROP_GETTER;
            propFlags &= ~JSPROP_ENUMERATE;

            AutoResolveName arn(ccx, idval);
            if(resolved)
                *resolved = JS_TRUE;
            return OBJ_DEFINE_PROPERTY(ccx, obj, id, JSVAL_VOID,
                                       (JSPropertyOp) funobj, nsnull,
                                       propFlags, nsnull);
        }

#ifdef XPC_IDISPATCH_SUPPORT
        // Check to see if there's an IDispatch tearoff     
        if(wrapperToReflectInterfaceNames &&
            XPCIDispatchExtension::DefineProperty(ccx, obj, 
                idval, wrapperToReflectInterfaceNames, propFlags, resolved))
            return JS_TRUE;
#endif
        
        if(resolved)
            *resolved = JS_FALSE;
        return JS_TRUE;
    }

    if(!member)
    {
        if(wrapperToReflectInterfaceNames)
        {
            XPCWrappedNativeTearOff* to =
              wrapperToReflectInterfaceNames->FindTearOff(ccx, iface, JS_TRUE);

            if(!to)
                return JS_FALSE;
            JSObject* jso = to->GetJSObject();
            if(!jso)
                return JS_FALSE;

            AutoResolveName arn(ccx, idval);
            if(resolved)
                *resolved = JS_TRUE;
            return JS_ValueToId(ccx, idval, &id) &&
                   OBJ_DEFINE_PROPERTY(ccx, obj, id, OBJECT_TO_JSVAL(jso),
                                       nsnull, nsnull,
                                       propFlags & ~JSPROP_ENUMERATE,
                                       nsnull);
        }
        if(resolved)
            *resolved = JS_FALSE;
        return JS_TRUE;
    }

    if(member->IsConstant())
    {
        jsval val;
        AutoResolveName arn(ccx, idval);
        if(resolved)
            *resolved = JS_TRUE;
        return member->GetValue(ccx, iface, &val) &&
               JS_ValueToId(ccx, idval, &id) &&
               OBJ_DEFINE_PROPERTY(ccx, obj, id, val, nsnull, nsnull,
                                   propFlags, nsnull);
    }

    if(idval == rt->GetStringJSVal(XPCJSRuntime::IDX_TO_STRING) ||
       idval == rt->GetStringJSVal(XPCJSRuntime::IDX_TO_SOURCE) ||
       (scriptableInfo &&
        scriptableInfo->GetFlags().DontEnumQueryInterface() &&
        idval == rt->GetStringJSVal(XPCJSRuntime::IDX_QUERY_INTERFACE)))
        propFlags &= ~JSPROP_ENUMERATE;

    JSObject* funobj;
    
    {
        // scoped gc protection of funval
        jsval funval;

        if(!member->GetValue(ccx, iface, &funval))
            return JS_FALSE;
    
        AUTO_MARK_JSVAL(ccx, funval);

        funobj = xpc_CloneJSFunction(ccx, JSVAL_TO_OBJECT(funval), obj);
        if(!funobj)
            return JS_FALSE;
    }

    // protect funobj until it is actually attached
    AUTO_MARK_JSVAL(ccx, OBJECT_TO_JSVAL(funobj));

#ifdef off_DEBUG_jband
    {
        static int cloneCount = 0;
        if(!(++cloneCount%10))
            printf("<><><> %d cloned functions created\n", cloneCount);
    }
#endif

    if(member->IsMethod())
    {
        AutoResolveName arn(ccx, idval);
        if(resolved)
            *resolved = JS_TRUE;
        return JS_ValueToId(ccx, idval, &id) &&
               OBJ_DEFINE_PROPERTY(ccx, obj, id, OBJECT_TO_JSVAL(funobj),
                                   nsnull, nsnull, propFlags, nsnull);
    }

    // else...

    NS_ASSERTION(member->IsAttribute(), "way broken!");

    propFlags |= JSPROP_GETTER | JSPROP_SHARED;
    if(member->IsWritableAttribute())
    {
        propFlags |= JSPROP_SETTER;
        propFlags &= ~JSPROP_READONLY;
    }

    AutoResolveName arn(ccx, idval);
    if(resolved)
        *resolved = JS_TRUE;
    return JS_ValueToId(ccx, idval, &id) &&
           OBJ_DEFINE_PROPERTY(ccx, obj, id, JSVAL_VOID,
                               (JSPropertyOp) funobj,
                               (JSPropertyOp) funobj,
                               propFlags, nsnull);
}

/***************************************************************************/
/***************************************************************************/

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_OnlyIWrite_PropertyStub(JSContext *cx, JSObject *obj, jsval idval, jsval *vp)
{
    CHECK_IDVAL(cx, idval);

    XPCCallContext ccx(JS_CALLER, cx, obj, nsnull, idval);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    // Allow only XPConnect to add the property
    if(ccx.GetResolveName() == idval)
        return JS_TRUE;

    return Throw(NS_ERROR_XPC_CANT_MODIFY_PROP_ON_WN, cx);
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_CannotModifyPropertyStub(JSContext *cx, JSObject *obj, jsval idval, jsval *vp)
{
    CHECK_IDVAL(cx, idval);
    return Throw(NS_ERROR_XPC_CANT_MODIFY_PROP_ON_WN, cx);
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_Shared_Convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    if(type == JSTYPE_OBJECT)
    {
        *vp = OBJECT_TO_JSVAL(obj);
        return JS_TRUE;
    }

    XPCCallContext ccx(JS_CALLER, cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    switch (type)
    {
        case JSTYPE_FUNCTION:
            {
                if(!ccx.GetTearOff())
                {
                    XPCNativeScriptableInfo* si = wrapper->GetScriptableInfo();
                    if(si && (si->GetFlags().WantCall() ||
                              si->GetFlags().WantConstruct()))
                    {
                        *vp = OBJECT_TO_JSVAL(obj);
                        return JS_TRUE;
                    }
                }
            }
            return Throw(NS_ERROR_XPC_CANT_CONVERT_WN_TO_FUN, cx);
        case JSTYPE_NUMBER:
            *vp = JS_GetNaNValue(cx);
            return JS_TRUE;
        case JSTYPE_BOOLEAN:
            *vp = JSVAL_TRUE;
            return JS_TRUE;
        case JSTYPE_VOID:
        case JSTYPE_STRING:
        {
            ccx.SetName(ccx.GetRuntime()->GetStringJSVal(XPCJSRuntime::IDX_TO_STRING));
            ccx.SetArgsAndResultPtr(0, nsnull, vp);

            XPCNativeMember* member = ccx.GetMember();
            if(member && member->IsMethod())
            {
                if(!XPCWrappedNative::CallMethod(ccx))
                    return JS_FALSE;

                if(JSVAL_IS_PRIMITIVE(*vp))
                    return JS_TRUE;
            }

            // else...
            return ToStringGuts(ccx);
        }
        default:
            NS_ERROR("bad type in conversion");
            return JS_FALSE;
    }
    NS_NOTREACHED("huh?");
    return JS_FALSE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_Shared_Enumerate(JSContext *cx, JSObject *obj)
{
    XPCCallContext ccx(JS_CALLER, cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    // Since we aren't going to enumerate tearoff names and the prototype
    // handles non-mutated members, we can do this potential short-circuit.
    if(!wrapper->HasMutatedSet())
        return JS_TRUE;

    // Since we might be using this in the helper case, we check to
    // see if this is all avoidable.

    if(wrapper->GetScriptableInfo() &&
       wrapper->GetScriptableInfo()->GetFlags().DontEnumStaticProps())
        return JS_TRUE;

    XPCNativeSet* set = wrapper->GetSet();
    XPCNativeSet* protoSet = wrapper->HasProto() ?
                                wrapper->GetProto()->GetSet() : nsnull;

    PRUint16 interface_count = set->GetInterfaceCount();
    XPCNativeInterface** interfaceArray = set->GetInterfaceArray();
    for(PRUint16 i = 0; i < interface_count; i++)
    {
        XPCNativeInterface* iface = interfaceArray[i];
#ifdef XPC_IDISPATCH_SUPPORT
        if(iface->GetIID()->Equals(NSID_IDISPATCH))
        {
            XPCIDispatchExtension::Enumerate(ccx, obj, wrapper);
            continue;
        }
#endif
        PRUint16 member_count = iface->GetMemberCount();
        for(PRUint16 k = 0; k < member_count; k++)
        {
            XPCNativeMember* member = iface->GetMemberAt(k);
            jsval name = member->GetName();

            // Skip if this member is going to come from the proto.
            PRUint16 index;
            if(protoSet &&
               protoSet->FindMember(name, nsnull, &index) && index == i)
                continue;
            if(!xpc_ForcePropertyResolve(cx, obj, name))
                return JS_FALSE;
        }
    }
    return JS_TRUE;
}

/***************************************************************************/

JS_STATIC_DLL_CALLBACK(void)
XPC_WN_NoHelper_Finalize(JSContext *cx, JSObject *obj)
{
    XPCWrappedNative* p = (XPCWrappedNative*) JS_GetPrivate(cx, obj);
    if(!p)
        return;
    p->FlatJSObjectFinalized(cx, obj);
}

static void
MarkScopeJSObjects(JSContext *cx, XPCWrappedNativeScope* scope, void *arg)
{
    NS_ASSERTION(scope, "bad scope");

    JSObject* obj;

    obj = scope->GetGlobalJSObject();
    NS_ASSERTION(scope, "bad scope JSObject");
    JS_MarkGCThing(cx, obj, "XPCWrappedNativeScope::mGlobalJSObject", arg);

    obj = scope->GetPrototypeJSObject();
    if(obj)
    {
        JS_MarkGCThing(cx, obj, "XPCWrappedNativeScope::mPrototypeJSObject", arg);
    }

    obj = scope->GetPrototypeJSFunction();
    if(obj)
    {
        JS_MarkGCThing(cx, obj, "XPCWrappedNativeScope::mPrototypeJSFunction", arg);
    }
}

void
xpc_MarkForValidWrapper(JSContext *cx, XPCWrappedNative* wrapper, void *arg)
{
    // NOTE: It might be nice to also do the wrapper->Mark() call here too.
    // That call marks the wrapper's and wrapper's proto's interface sets.
    // We currently do that in the GC callback code. The reason we don't do that
    // here is because the bits used in that marking do unpleasant things to the
    // member counts in the interface and interface set objects. Those counts
    // are used in the DealWithDyingGCThings calls that are part of this JS GC
    // marking phase. By doing these calls later during our GC callback we 
    // avoid that problem. Arguably this could be changed. But it ain't broke.

    // However, we do need to call the wrapper's MarkBeforeJSFinalize so that
    // it can be sure that its (potentially shared) JSClass gets marked. The
    // danger is that a live wrapper might not be in a wrapper map and thus
    // won't be fully marked in the GC callback. This can happen if there is
    // a security exception during wrapper creation or if during wrapper
    // creation it is determined that the wrapper is not needed. In those cases
    // the wrapper can never actually be used from JS code - so resources like
    // the interface set will never be accessed. But the JS engine will still
    // need to use the JSClass. So, some marking is required for protection.

    wrapper->MarkBeforeJSFinalize(cx);
     
    MarkScopeJSObjects(cx, wrapper->GetScope(), arg);
}

JS_STATIC_DLL_CALLBACK(uint32)
XPC_WN_Shared_Mark(JSContext *cx, JSObject *obj, void *arg)
{
    XPCWrappedNative* wrapper =
        XPCWrappedNative::GetWrappedNativeOfJSObject(cx, obj);

    if(wrapper && wrapper->IsValid())
        xpc_MarkForValidWrapper(cx, wrapper, arg);
    return 1;
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_NoHelper_Resolve(JSContext *cx, JSObject *obj, jsval idval)
{
    CHECK_IDVAL(cx, idval);

    XPCCallContext ccx(JS_CALLER, cx, obj, nsnull, idval);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    XPCNativeSet* set = ccx.GetSet();
    if(!set)
        return JS_TRUE;

    // Don't resolve properties that are on our prototype.
    if(ccx.GetInterface() && !ccx.GetStaticMemberIsLocal())
        return JS_TRUE;

    return DefinePropertyIfFound(ccx, obj, idval,
                                 set, nsnull, nsnull, wrapper->GetScope(),
                                 JS_TRUE, wrapper, wrapper, nsnull,
                                 JSPROP_ENUMERATE |
                                 JSPROP_READONLY |
                                 JSPROP_PERMANENT, nsnull);
}

nsISupports *
XPC_GetIdentityObject(JSContext *cx, JSObject *obj)
{
    XPCWrappedNative *wrapper;

    if(XPCNativeWrapper::IsNativeWrapper(cx, obj))
        wrapper = XPCNativeWrapper::GetWrappedNative(cx, obj);
    else
        wrapper = XPCWrappedNative::GetWrappedNativeOfJSObject(cx, obj);

    if(!wrapper) {
        JSObject *unsafeObj = XPC_SJOW_GetUnsafeObject(cx, obj);
        if(unsafeObj)
            return XPC_GetIdentityObject(cx, unsafeObj);

        return nsnull;
    }

    return wrapper->GetIdentityObject();
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_Equality(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
    *bp = JS_FALSE;

    XPCWrappedNative *wrapper =
        XPCWrappedNative::GetWrappedNativeOfJSObject(cx, obj);
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    XPCNativeScriptableInfo* si = wrapper->GetScriptableInfo();
    if(si && si->GetFlags().WantEquality())
    {
        nsresult rv = si->GetCallback()->Equality(wrapper, cx, obj, v, bp);
        if(NS_FAILED(rv))
            return Throw(rv, cx);

        if (!*bp && !JSVAL_IS_PRIMITIVE(v) &&
            IsXPCSafeJSObjectWrapperClass(JS_GET_CLASS(cx,
                                                       JSVAL_TO_OBJECT(v)))) {
            v = OBJECT_TO_JSVAL(XPC_SJOW_GetUnsafeObject(cx,
                                                         JSVAL_TO_OBJECT(v)));

            rv = si->GetCallback()->Equality(wrapper, cx, obj, v, bp);
            if(NS_FAILED(rv))
                return Throw(rv, cx);
        }
    }
    else if(!JSVAL_IS_PRIMITIVE(v))
    {
        JSObject *other = JSVAL_TO_OBJECT(v);

        *bp = (obj == other ||
               XPC_GetIdentityObject(cx, obj) ==
               XPC_GetIdentityObject(cx, other));
    }

    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSObject *)
XPC_WN_OuterObject(JSContext *cx, JSObject *obj)
{
    XPCWrappedNative *wrapper =
        XPCWrappedNative::GetWrappedNativeOfJSObject(cx, obj);
    if(!wrapper)
    {
        Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);

        return nsnull;
    }

    if(!wrapper->IsValid())
    {
        Throw(NS_ERROR_XPC_HAS_BEEN_SHUTDOWN, cx);

        return nsnull;
    }

    XPCNativeScriptableInfo* si = wrapper->GetScriptableInfo();
    if(si && si->GetFlags().WantOuterObject())
    {
        JSObject *newThis;
        nsresult rv =
            si->GetCallback()->OuterObject(wrapper, cx, obj, &newThis);

        if(NS_FAILED(rv))
        {
            Throw(rv, cx);

            return nsnull;
        }

        obj = newThis;
    }

    return obj;
}

JS_STATIC_DLL_CALLBACK(JSObject *)
XPC_WN_InnerObject(JSContext *cx, JSObject *obj)
{
    XPCWrappedNative *wrapper =
        XPCWrappedNative::GetWrappedNativeOfJSObject(cx, obj);
    if(!wrapper)
    {
        Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);

        return nsnull;
    }

    if(!wrapper->IsValid())
    {
        Throw(NS_ERROR_XPC_HAS_BEEN_SHUTDOWN, cx);

        return nsnull;
    }

    XPCNativeScriptableInfo* si = wrapper->GetScriptableInfo();
    if(si && si->GetFlags().WantInnerObject())
    {
        JSObject *newThis;
        nsresult rv =
            si->GetCallback()->InnerObject(wrapper, cx, obj, &newThis);

        if(NS_FAILED(rv))
        {
            Throw(rv, cx);

            return nsnull;
        }

        obj = newThis;
    }

    return obj;
}

JSExtendedClass XPC_WN_NoHelper_JSClass = {
    {
        "XPCWrappedNative_NoHelper",    // name;
        JSCLASS_HAS_PRIVATE |
        JSCLASS_PRIVATE_IS_NSISUPPORTS |
        JSCLASS_IS_EXTENDED, // flags;

        /* Mandatory non-null function pointer members. */
        XPC_WN_OnlyIWrite_PropertyStub, // addProperty;
        XPC_WN_CannotModifyPropertyStub,// delProperty;
        JS_PropertyStub,                // getProperty;
        XPC_WN_OnlyIWrite_PropertyStub, // setProperty;

        XPC_WN_Shared_Enumerate,        // enumerate;
        XPC_WN_NoHelper_Resolve,        // resolve;
        XPC_WN_Shared_Convert,          // convert;
        XPC_WN_NoHelper_Finalize,       // finalize;

        /* Optionally non-null members start here. */
        nsnull,                         // getObjectOps;
        nsnull,                         // checkAccess;
        nsnull,                         // call;
        nsnull,                         // construct;
        nsnull,                         // xdrObject;
        nsnull,                         // hasInstance;
        XPC_WN_Shared_Mark,             // mark;
        nsnull                          // spare;
    },
    XPC_WN_Equality,
    XPC_WN_OuterObject,
    XPC_WN_InnerObject,
    nsnull,nsnull,nsnull,nsnull,nsnull
};


/***************************************************************************/

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_MaybeResolvingPropertyStub(JSContext *cx, JSObject *obj, jsval idval, jsval *vp)
{
    CHECK_IDVAL(cx, idval);
    XPCCallContext ccx(JS_CALLER, cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    if(ccx.GetResolvingWrapper() == wrapper)
        return JS_TRUE;
    return Throw(NS_ERROR_XPC_CANT_MODIFY_PROP_ON_WN, cx);
}

// macro fun!
#define PRE_HELPER_STUB                                                      \
    XPCWrappedNative* wrapper =                                              \
        XPCWrappedNative::GetWrappedNativeOfJSObject(cx, obj);               \
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);                            \
    PRBool retval = JS_TRUE;                                                 \
    nsresult rv = wrapper->GetScriptableCallback()->

#define POST_HELPER_STUB                                                     \
    if(NS_FAILED(rv))                                                        \
        return Throw(rv, cx);                                                \
    return retval;

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_Helper_AddProperty(JSContext *cx, JSObject *obj, jsval idval, jsval *vp)
{
    PRE_HELPER_STUB
    AddProperty(wrapper, cx, obj, idval, vp, &retval);
    POST_HELPER_STUB
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_Helper_DelProperty(JSContext *cx, JSObject *obj, jsval idval, jsval *vp)
{
    PRE_HELPER_STUB
    DelProperty(wrapper, cx, obj, idval, vp, &retval);
    POST_HELPER_STUB
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_Helper_GetProperty(JSContext *cx, JSObject *obj, jsval idval, jsval *vp)
{
    PRE_HELPER_STUB
    GetProperty(wrapper, cx, obj, idval, vp, &retval);
    POST_HELPER_STUB
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_Helper_SetProperty(JSContext *cx, JSObject *obj, jsval idval, jsval *vp)
{
    PRE_HELPER_STUB
    SetProperty(wrapper, cx, obj, idval, vp, &retval);
    POST_HELPER_STUB
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_Helper_Convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    PRE_HELPER_STUB
    Convert(wrapper, cx, obj, type, vp, &retval);
    POST_HELPER_STUB
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_Helper_CheckAccess(JSContext *cx, JSObject *obj, jsval idval,
                          JSAccessMode mode, jsval *vp)
{
    CHECK_IDVAL(cx, idval);
    PRE_HELPER_STUB
    CheckAccess(wrapper, cx, obj, idval, mode, vp, &retval);
    POST_HELPER_STUB
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_Helper_Call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                   jsval *rval)
{
    // this is a hack to get the obj of the actual object not the object
    // that JS thinks is the 'this' (which it passes as 'obj').
    if(!(obj = (JSObject*)argv[-2]))
        return JS_FALSE;

    PRE_HELPER_STUB
    Call(wrapper, cx, obj, argc, argv, rval, &retval);
    POST_HELPER_STUB
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_Helper_Construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                        jsval *rval)
{
    // this is a hack to get the obj of the actual object not the object
    // that JS thinks is the 'this' (which it passes as 'obj').
    if(!(obj = (JSObject*)argv[-2]))
        return JS_FALSE;

    PRE_HELPER_STUB
    Construct(wrapper, cx, obj, argc, argv, rval, &retval);
    POST_HELPER_STUB
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_Helper_HasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
    PRE_HELPER_STUB
    HasInstance(wrapper, cx, obj, v, bp, &retval);
    POST_HELPER_STUB
}

JS_STATIC_DLL_CALLBACK(void)
XPC_WN_Helper_Finalize(JSContext *cx, JSObject *obj)
{
    XPCWrappedNative* wrapper = (XPCWrappedNative*) JS_GetPrivate(cx, obj);
    if(!wrapper)
        return;
    wrapper->GetScriptableCallback()->Finalize(wrapper, cx, obj);
    wrapper->FlatJSObjectFinalized(cx, obj);
}

JS_STATIC_DLL_CALLBACK(uint32)
XPC_WN_Helper_Mark(JSContext *cx, JSObject *obj, void *arg)
{
    PRUint32 ignored = 0;
    XPCWrappedNative* wrapper =
        XPCWrappedNative::GetWrappedNativeOfJSObject(cx, obj);
    if(wrapper && wrapper->IsValid())
    {
        wrapper->GetScriptableCallback()->Mark(wrapper, cx, obj, arg, &ignored);
        xpc_MarkForValidWrapper(cx, wrapper, arg);
    }
    return (uint32) ignored;
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_Helper_NewResolve(JSContext *cx, JSObject *obj, jsval idval, uintN flags,
                         JSObject **objp)
{
    CHECK_IDVAL(cx, idval);

    XPCCallContext ccx(JS_CALLER, cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    jsval old = ccx.SetResolveName(idval);

    nsresult rv = NS_OK;
    JSBool retval = JS_TRUE;
    JSObject* obj2FromScriptable = nsnull;

    XPCNativeScriptableInfo* si = wrapper->GetScriptableInfo();
    if(si && si->GetFlags().WantNewResolve())
    {
        XPCWrappedNative* oldResolvingWrapper;
        JSBool allowPropMods = si->GetFlags().AllowPropModsDuringResolve();

        if(allowPropMods)
            oldResolvingWrapper = ccx.SetResolvingWrapper(wrapper);

        rv = si->GetCallback()->NewResolve(wrapper, cx, obj, idval, flags,
                                             &obj2FromScriptable, &retval);

        if(allowPropMods)
            (void)ccx.SetResolvingWrapper(oldResolvingWrapper);
    }

    old = ccx.SetResolveName(old);
    NS_ASSERTION(old == idval, "bad nest");

    if(NS_FAILED(rv))
    {
        return Throw(rv, cx);
    }

    if(obj2FromScriptable)
    {
        *objp = obj2FromScriptable;
    }
    else if(wrapper->HasMutatedSet())
    {
        // We are here if scriptable did not resolve this property and
        // it *might* be in the instance set but not the proto set.

        XPCNativeSet* set = wrapper->GetSet();
        XPCNativeSet* protoSet = wrapper->HasProto() ?
                                    wrapper->GetProto()->GetSet() : nsnull;
        XPCNativeMember* member;
        XPCNativeInterface* iface;
        JSBool IsLocal;

        if(set->FindMember(idval, &member, &iface, protoSet, &IsLocal) &&
           IsLocal)
        {
            XPCWrappedNative* oldResolvingWrapper;

            XPCNativeScriptableFlags siFlags(0);
            if(si)
                siFlags = si->GetFlags();

            uintN enumFlag =
                siFlags.DontEnumStaticProps() ? 0 : JSPROP_ENUMERATE;

            XPCWrappedNative* wrapperForInterfaceNames =
                siFlags.DontReflectInterfaceNames() ? nsnull : wrapper;

            JSBool resolved;
            oldResolvingWrapper = ccx.SetResolvingWrapper(wrapper);
            retval = DefinePropertyIfFound(ccx, obj, idval,
                                           set, iface, member,
                                           wrapper->GetScope(),
                                           JS_FALSE,
                                           wrapperForInterfaceNames,
                                           nsnull, si,
                                           enumFlag, &resolved);
            (void)ccx.SetResolvingWrapper(oldResolvingWrapper);
            if(retval && resolved)
                *objp = obj;
        }
    }

    return retval;
}

/***************************************************************************/

extern "C" JS_IMPORT_DATA(JSObjectOps) js_ObjectOps;

static JSObjectOps XPC_WN_WithCall_JSOps;
static JSObjectOps XPC_WN_NoCall_JSOps;

/*
    Here are the enumerator cases:

    set jsclass enumerate to stub (unless noted otherwise)

    if( helper wants new enumerate )
        if( DONT_ENUM_STATICS )
            forward to scriptable enumerate
        else
            if( set not mutated )
                forward to scriptable enumerate
            else
                call shared enumerate
                forward to scriptable enumerate
    else if( helper wants old enumerate )
        use this JSOp
        if( DONT_ENUM_STATICS )
            call scriptable enumerate
            call stub
        else
            if( set not mutated )
                call scriptable enumerate
                call stub
            else
                call shared enumerate
                call scriptable enumerate
                call stub

    else //... if( helper wants NO enumerate )
        if( DONT_ENUM_STATICS )
            use enumerate stub - don't use this JSOp thing at all
        else
            do shared enumerate - don't use this JSOp thing at all
*/

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_JSOp_Enumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
                      jsval *statep, jsid *idp)
{
    XPCCallContext ccx(JS_CALLER, cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    XPCNativeScriptableInfo* si = wrapper->GetScriptableInfo();
    if(!si)
        return Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);

    PRBool retval = JS_TRUE;
    nsresult rv;

    if(si->GetFlags().WantNewEnumerate())
    {
        if(enum_op == JSENUMERATE_INIT &&
           !si->GetFlags().DontEnumStaticProps() &&
           wrapper->HasMutatedSet() &&
           !XPC_WN_Shared_Enumerate(cx, obj))
        {
            *statep = JSVAL_NULL;
            return JS_FALSE;
        }

        // XXX Might we really need to wrap this call and *also* call
        // js_ObjectOps.enumerate ???

        rv = si->GetCallback()->
            NewEnumerate(wrapper, cx, obj, enum_op, statep, idp, &retval);
        
        if(enum_op == JSENUMERATE_INIT && (NS_FAILED(rv) || !retval))
            *statep = JSVAL_NULL;
        
        if(NS_FAILED(rv))
            return Throw(rv, cx);
        return retval;
    }

    if(si->GetFlags().WantEnumerate())
    {
        if(enum_op == JSENUMERATE_INIT)
        {
            if(!si->GetFlags().DontEnumStaticProps() &&
               wrapper->HasMutatedSet() &&
               !XPC_WN_Shared_Enumerate(cx, obj))
            {
                *statep = JSVAL_NULL;
                return JS_FALSE;
            }
            rv = si->GetCallback()->
                Enumerate(wrapper, cx, obj, &retval);

            if(NS_FAILED(rv) || !retval)
                *statep = JSVAL_NULL;

            if(NS_FAILED(rv))
                return Throw(rv, cx);
            if(!retval)
                return JS_FALSE;
            // Then fall through and call js_ObjectOps.enumerate...
        }
    }

    // else call js_ObjectOps.enumerate...

    return js_ObjectOps.enumerate(cx, obj, enum_op, statep, idp);
}

JS_STATIC_DLL_CALLBACK(void)
XPC_WN_JSOp_Clear(JSContext *cx, JSObject *obj)
{
    // If our scope is cleared, make sure we clear the scope of our
    // native wrapper as well.
    XPCWrappedNative *wrapper =
        XPCWrappedNative::GetWrappedNativeOfJSObject(cx, obj);

    if(wrapper && wrapper->IsValid())
    {
        XPCNativeWrapper::ClearWrappedNativeScopes(cx, wrapper);
    }

    js_ObjectOps.clear(cx, obj);
}

JSObjectOps * JS_DLL_CALLBACK
XPC_WN_GetObjectOpsNoCall(JSContext *cx, JSClass *clazz)
{
    return &XPC_WN_NoCall_JSOps;
}

JSObjectOps * JS_DLL_CALLBACK
XPC_WN_GetObjectOpsWithCall(JSContext *cx, JSClass *clazz)
{
    return &XPC_WN_WithCall_JSOps;
}

JSBool xpc_InitWrappedNativeJSOps()
{
    if(!XPC_WN_NoCall_JSOps.newObjectMap)
    {
        memcpy(&XPC_WN_NoCall_JSOps, &js_ObjectOps, sizeof(JSObjectOps));
        XPC_WN_NoCall_JSOps.enumerate = XPC_WN_JSOp_Enumerate;

        memcpy(&XPC_WN_WithCall_JSOps, &js_ObjectOps, sizeof(JSObjectOps));
        XPC_WN_WithCall_JSOps.enumerate = XPC_WN_JSOp_Enumerate;
        XPC_WN_WithCall_JSOps.clear = XPC_WN_JSOp_Clear;

        XPC_WN_NoCall_JSOps.call = nsnull;
        XPC_WN_NoCall_JSOps.construct = nsnull;
        XPC_WN_NoCall_JSOps.clear = XPC_WN_JSOp_Clear;
    }
    return JS_TRUE;
}

/***************************************************************************/

// static
XPCNativeScriptableInfo*
XPCNativeScriptableInfo::Construct(XPCCallContext& ccx,
                                   JSBool isGlobal,
                                   const XPCNativeScriptableCreateInfo* sci)
{
    NS_ASSERTION(sci->GetCallback(), "bad param");
    NS_ASSERTION(sci, "bad param");

    XPCNativeScriptableInfo* newObj =
        new XPCNativeScriptableInfo(sci->GetCallback());
    if(!newObj)
        return nsnull;

    char* name = nsnull;
    if(NS_FAILED(sci->GetCallback()->GetClassName(&name)) || !name)
    {
        delete newObj;
        return nsnull;
    }

    JSBool success;

    XPCJSRuntime* rt = ccx.GetRuntime();
    XPCNativeScriptableSharedMap* map = rt->GetNativeScriptableSharedMap();
    {   // scoped lock
        XPCAutoLock lock(rt->GetMapLock());
        success = map->GetNewOrUsed(sci->GetFlags(), name, isGlobal, newObj);
    }

    if(!success)
    {
        delete newObj;
        return nsnull;
    }

    return newObj;
}

void
XPCNativeScriptableShared::PopulateJSClass(JSBool isGlobal)
{
    NS_ASSERTION(mJSClass.base.name, "bad state!");

    mJSClass.base.flags = JSCLASS_HAS_PRIVATE |
                          JSCLASS_PRIVATE_IS_NSISUPPORTS |
                          JSCLASS_NEW_RESOLVE |
                          JSCLASS_IS_EXTENDED;

    if(isGlobal)
        mJSClass.base.flags |= JSCLASS_GLOBAL_FLAGS;

    if(mFlags.WantAddProperty())
        mJSClass.base.addProperty = XPC_WN_Helper_AddProperty;
    else if(mFlags.UseJSStubForAddProperty())
        mJSClass.base.addProperty = JS_PropertyStub;
    else if(mFlags.AllowPropModsDuringResolve())
        mJSClass.base.addProperty = XPC_WN_MaybeResolvingPropertyStub;
    else
        mJSClass.base.addProperty = XPC_WN_CannotModifyPropertyStub;

    if(mFlags.WantDelProperty())
        mJSClass.base.delProperty = XPC_WN_Helper_DelProperty;
    else if(mFlags.UseJSStubForDelProperty())
        mJSClass.base.delProperty = JS_PropertyStub;
    else if(mFlags.AllowPropModsDuringResolve())
        mJSClass.base.delProperty = XPC_WN_MaybeResolvingPropertyStub;
    else
        mJSClass.base.delProperty = XPC_WN_CannotModifyPropertyStub;

    if(mFlags.WantGetProperty())
        mJSClass.base.getProperty = XPC_WN_Helper_GetProperty;
    else
        mJSClass.base.getProperty = JS_PropertyStub;

    if(mFlags.WantSetProperty())
        mJSClass.base.setProperty = XPC_WN_Helper_SetProperty;
    else if(mFlags.UseJSStubForSetProperty())
        mJSClass.base.setProperty = JS_PropertyStub;
    else if(mFlags.AllowPropModsDuringResolve())
        mJSClass.base.setProperty = XPC_WN_MaybeResolvingPropertyStub;
    else
        mJSClass.base.setProperty = XPC_WN_CannotModifyPropertyStub;

    // We figure out most of the enumerate strategy at call time.

    if(mFlags.WantNewEnumerate() || mFlags.WantEnumerate() ||
       mFlags.DontEnumStaticProps())
        mJSClass.base.enumerate = JS_EnumerateStub;
    else
        mJSClass.base.enumerate = XPC_WN_Shared_Enumerate;

    // We have to figure out resolve strategy at call time
    mJSClass.base.resolve = (JSResolveOp) XPC_WN_Helper_NewResolve;

    if(mFlags.WantConvert())
        mJSClass.base.convert = XPC_WN_Helper_Convert;
    else
        mJSClass.base.convert = XPC_WN_Shared_Convert;

    if(mFlags.WantFinalize())
        mJSClass.base.finalize = XPC_WN_Helper_Finalize;
    else
        mJSClass.base.finalize = XPC_WN_NoHelper_Finalize;

    // We let the rest default to nsnull unless the helper wants them...
    if(mFlags.WantCheckAccess())
        mJSClass.base.checkAccess = XPC_WN_Helper_CheckAccess;

    // Note that we *must* set
    //   mJSClass.base.getObjectOps = XPC_WN_GetObjectOpsNoCall
    // or
    //   mJSClass.base.getObjectOps = XPC_WN_GetObjectOpsWithCall
    // (even for the cases were it does not do much) because with these
    // dynamically generated JSClasses, the code in
    // XPCWrappedNative::GetWrappedNativeOfJSObject() needs to look for
    // that this callback pointer in order to identify that a given
    // JSObject represents a wrapper.

    if(mFlags.WantCall() || mFlags.WantConstruct())
    {
        mJSClass.base.getObjectOps = XPC_WN_GetObjectOpsWithCall;
        if(mFlags.WantCall())
            mJSClass.base.call = XPC_WN_Helper_Call;
        if(mFlags.WantConstruct())
            mJSClass.base.construct = XPC_WN_Helper_Construct;
    }
    else
    {
        mJSClass.base.getObjectOps = XPC_WN_GetObjectOpsNoCall;
    }

    if(mFlags.WantHasInstance())
        mJSClass.base.hasInstance = XPC_WN_Helper_HasInstance;

    if(mFlags.WantMark())
        mJSClass.base.mark = XPC_WN_Helper_Mark;
    else
        mJSClass.base.mark = XPC_WN_Shared_Mark;

    mJSClass.equality = XPC_WN_Equality;
    mJSClass.outerObject = XPC_WN_OuterObject;
    mJSClass.innerObject = XPC_WN_InnerObject;
}

/***************************************************************************/
/***************************************************************************/

JSBool JS_DLL_CALLBACK
XPC_WN_CallMethod(JSContext *cx, JSObject *obj,
                  uintN argc, jsval *argv, jsval *vp)
{
    NS_ASSERTION(JS_TypeOfValue(cx, argv[-2]) == JSTYPE_FUNCTION, "bad function");
    JSObject* funobj = JSVAL_TO_OBJECT(argv[-2]);
    XPCCallContext ccx(JS_CALLER, cx, obj, funobj, 0, argc, argv, vp);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    XPCNativeInterface* iface;
    XPCNativeMember*    member;

    if(!XPCNativeMember::GetCallInfo(ccx, funobj, &iface, &member))
        return Throw(NS_ERROR_XPC_CANT_GET_METHOD_INFO, cx);
    ccx.SetCallInfo(iface, member, JS_FALSE);
    return XPCWrappedNative::CallMethod(ccx);
}

JSBool JS_DLL_CALLBACK
XPC_WN_GetterSetter(JSContext *cx, JSObject *obj,
                    uintN argc, jsval *argv, jsval *vp)
{
    NS_ASSERTION(JS_TypeOfValue(cx, argv[-2]) == JSTYPE_FUNCTION, "bad function");
    JSObject* funobj = JSVAL_TO_OBJECT(argv[-2]);

    XPCCallContext ccx(JS_CALLER, cx, obj, funobj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    XPCNativeInterface* iface;
    XPCNativeMember*    member;

    if(!XPCNativeMember::GetCallInfo(ccx, funobj, &iface, &member))
        return Throw(NS_ERROR_XPC_CANT_GET_METHOD_INFO, cx);

    ccx.SetArgsAndResultPtr(argc, argv, vp);
    if(argc)
    {
        ccx.SetCallInfo(iface, member, JS_TRUE);
        JSBool retval = XPCWrappedNative::SetAttribute(ccx);
        if(retval && vp)
            *vp = argv[0];
        return retval;
    }
    // else...

    ccx.SetCallInfo(iface, member, JS_FALSE);
    return XPCWrappedNative::GetAttribute(ccx);
}

/***************************************************************************/

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_Shared_Proto_Enumerate(JSContext *cx, JSObject *obj)
{
    NS_ASSERTION(
        JS_InstanceOf(cx, obj, &XPC_WN_ModsAllowed_Proto_JSClass, nsnull) ||
        JS_InstanceOf(cx, obj, &XPC_WN_NoMods_Proto_JSClass, nsnull),
                 "bad proto");
    XPCWrappedNativeProto* self = (XPCWrappedNativeProto*) JS_GetPrivate(cx, obj);
    if(!self)
        return JS_FALSE;

    if(self->GetScriptableInfo() &&
       self->GetScriptableInfo()->GetFlags().DontEnumStaticProps())
        return JS_TRUE;

    XPCNativeSet* set = self->GetSet();
    if(!set)
        return JS_FALSE;

    XPCCallContext ccx(JS_CALLER, cx);
    if(!ccx.IsValid())
        return JS_FALSE;

    PRUint16 interface_count = set->GetInterfaceCount();
    XPCNativeInterface** interfaceArray = set->GetInterfaceArray();
    for(PRUint16 i = 0; i < interface_count; i++)
    {
        XPCNativeInterface* iface = interfaceArray[i];
        PRUint16 member_count = iface->GetMemberCount();

        for(PRUint16 k = 0; k < member_count; k++)
        {
            if(!xpc_ForcePropertyResolve(cx, obj, iface->GetMemberAt(k)->GetName()))
                return JS_FALSE;
        }
    }

    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_Shared_Proto_Convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    // XXX ?
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(void)
XPC_WN_Shared_Proto_Finalize(JSContext *cx, JSObject *obj)
{
    // This can be null if xpc shutdown has already happened
    XPCWrappedNativeProto* p = (XPCWrappedNativeProto*) JS_GetPrivate(cx, obj);
    if(p)
        p->JSProtoObjectFinalized(cx, obj);
}

JS_STATIC_DLL_CALLBACK(uint32)
XPC_WN_Shared_Proto_Mark(JSContext *cx, JSObject *obj, void *arg)
{
    // This can be null if xpc shutdown has already happened
    XPCWrappedNativeProto* p = (XPCWrappedNativeProto*) JS_GetPrivate(cx, obj);
    if(p)
        MarkScopeJSObjects(cx, p->GetScope(), arg);
    return 1;
}

/*****************************************************/

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_ModsAllowed_Proto_Resolve(JSContext *cx, JSObject *obj, jsval idval)
{
    CHECK_IDVAL(cx, idval);

    NS_ASSERTION(
        JS_InstanceOf(cx, obj, &XPC_WN_ModsAllowed_Proto_JSClass, nsnull),
                 "bad proto");

    XPCWrappedNativeProto* self = (XPCWrappedNativeProto*) JS_GetPrivate(cx, obj);
    if(!self)
        return JS_FALSE;

    XPCCallContext ccx(JS_CALLER, cx);
    if(!ccx.IsValid())
        return JS_FALSE;

    XPCNativeScriptableInfo* si = self->GetScriptableInfo();
    uintN enumFlag = (si && si->GetFlags().DontEnumStaticProps()) ?
                                                0 : JSPROP_ENUMERATE;

    return DefinePropertyIfFound(ccx, obj, idval,
                                 self->GetSet(), nsnull, nsnull,
                                 self->GetScope(),
                                 JS_TRUE, nsnull, nsnull, si,
                                 enumFlag, nsnull);
}


JSClass XPC_WN_ModsAllowed_Proto_JSClass = {
    "XPC_WN_ModsAllowed_Proto_JSClass", // name;
    JSCLASS_HAS_PRIVATE,                // flags;

    /* Mandatory non-null function pointer members. */
    JS_PropertyStub,                // addProperty;
    JS_PropertyStub,                // delProperty;
    JS_PropertyStub,                // getProperty;
    JS_PropertyStub,                // setProperty;
    XPC_WN_Shared_Proto_Enumerate,         // enumerate;
    XPC_WN_ModsAllowed_Proto_Resolve,      // resolve;
    XPC_WN_Shared_Proto_Convert,           // convert;
    XPC_WN_Shared_Proto_Finalize,          // finalize;

    /* Optionally non-null members start here. */
    nsnull,                         // getObjectOps;
    nsnull,                         // checkAccess;
    nsnull,                         // call;
    nsnull,                         // construct;
    nsnull,                         // xdrObject;
    nsnull,                         // hasInstance;
    XPC_WN_Shared_Proto_Mark,       // mark;
    nsnull                          // spare;
};

/***************************************************************************/

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_OnlyIWrite_Proto_PropertyStub(JSContext *cx, JSObject *obj, jsval idval, jsval *vp)
{
    CHECK_IDVAL(cx, idval);

    NS_ASSERTION(
        JS_InstanceOf(cx, obj, &XPC_WN_NoMods_Proto_JSClass, nsnull),
                 "bad proto");

    XPCWrappedNativeProto* self = (XPCWrappedNativeProto*) JS_GetPrivate(cx, obj);
    if(!self)
        return JS_FALSE;

    XPCCallContext ccx(JS_CALLER, cx);
    if(!ccx.IsValid())
        return JS_FALSE;

    // Allow XPConnect to add the property only
    if(ccx.GetResolveName() == idval)
        return JS_TRUE;

    return Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_NoMods_Proto_Resolve(JSContext *cx, JSObject *obj, jsval idval)
{
    CHECK_IDVAL(cx, idval);

    NS_ASSERTION(
        JS_InstanceOf(cx, obj, &XPC_WN_NoMods_Proto_JSClass, nsnull),
                 "bad proto");

    XPCWrappedNativeProto* self = (XPCWrappedNativeProto*) JS_GetPrivate(cx, obj);
    if(!self)
        return JS_FALSE;

    XPCCallContext ccx(JS_CALLER, cx);
    if(!ccx.IsValid())
        return JS_FALSE;

    XPCNativeScriptableInfo* si = self->GetScriptableInfo();
    uintN enumFlag = (si && si->GetFlags().DontEnumStaticProps()) ?
                                                0 : JSPROP_ENUMERATE;

    return DefinePropertyIfFound(ccx, obj, idval,
                                 self->GetSet(), nsnull, nsnull,
                                 self->GetScope(),
                                 JS_TRUE, nsnull, nsnull, si,
                                 JSPROP_READONLY |
                                 JSPROP_PERMANENT |
                                 enumFlag, nsnull);
}

JSClass XPC_WN_NoMods_Proto_JSClass = {
    "XPC_WN_NoMods_Proto_JSClass",      // name;
    JSCLASS_HAS_PRIVATE,                // flags;

    /* Mandatory non-null function pointer members. */
    XPC_WN_OnlyIWrite_Proto_PropertyStub,  // addProperty;
    XPC_WN_CannotModifyPropertyStub,       // delProperty;
    JS_PropertyStub,                       // getProperty;
    XPC_WN_OnlyIWrite_Proto_PropertyStub,  // setProperty;
    XPC_WN_Shared_Proto_Enumerate,         // enumerate;
    XPC_WN_NoMods_Proto_Resolve,           // resolve;
    XPC_WN_Shared_Proto_Convert,           // convert;
    XPC_WN_Shared_Proto_Finalize,          // finalize;

    /* Optionally non-null members start here. */
    nsnull,                         // getObjectOps;
    nsnull,                         // checkAccess;
    nsnull,                         // call;
    nsnull,                         // construct;
    nsnull,                         // xdrObject;
    nsnull,                         // hasInstance;
    XPC_WN_Shared_Proto_Mark,       // mark;
    nsnull                          // spare;
};

/***************************************************************************/

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_TearOff_Enumerate(JSContext *cx, JSObject *obj)
{
    XPCCallContext ccx(JS_CALLER, cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    XPCWrappedNativeTearOff* to = ccx.GetTearOff();
    XPCNativeInterface* iface;

    if(!to || nsnull == (iface = to->GetInterface()))
        return Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);

    PRUint16 member_count = iface->GetMemberCount();
    for(PRUint16 k = 0; k < member_count; k++)
    {
        if(!xpc_ForcePropertyResolve(cx, obj, iface->GetMemberAt(k)->GetName()))
            return JS_FALSE;
    }

    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
XPC_WN_TearOff_Resolve(JSContext *cx, JSObject *obj, jsval idval)
{
    CHECK_IDVAL(cx, idval);

    XPCCallContext ccx(JS_CALLER, cx, obj);
    XPCWrappedNative* wrapper = ccx.GetWrapper();
    THROW_AND_RETURN_IF_BAD_WRAPPER(cx, wrapper);

    XPCWrappedNativeTearOff* to = ccx.GetTearOff();
    XPCNativeInterface* iface;

    if(!to || nsnull == (iface = to->GetInterface()))
        return Throw(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);

    return DefinePropertyIfFound(ccx, obj, idval, nsnull, iface, nsnull,
                                 wrapper->GetScope(),
                                 JS_TRUE, nsnull, nsnull, nsnull,
                                 JSPROP_READONLY |
                                 JSPROP_PERMANENT |
                                 JSPROP_ENUMERATE, nsnull);
}

JS_STATIC_DLL_CALLBACK(void)
XPC_WN_TearOff_Finalize(JSContext *cx, JSObject *obj)
{
    XPCWrappedNativeTearOff* p = (XPCWrappedNativeTearOff*)
        JS_GetPrivate(cx, obj);
    if(!p)
        return;
    p->JSObjectFinalized();
}

JSClass XPC_WN_Tearoff_JSClass = {
    "WrappedNative_TearOff",            // name;
    JSCLASS_HAS_PRIVATE,                // flags;

    /* Mandatory non-null function pointer members. */
    XPC_WN_OnlyIWrite_PropertyStub,     // addProperty;
    XPC_WN_CannotModifyPropertyStub,    // delProperty;
    JS_PropertyStub,                    // getProperty;
    XPC_WN_OnlyIWrite_PropertyStub,     // setProperty;
    XPC_WN_TearOff_Enumerate,           // enumerate;
    XPC_WN_TearOff_Resolve,             // resolve;
    XPC_WN_Shared_Convert,              // convert;
    XPC_WN_TearOff_Finalize,            // finalize;

    /* Optionally non-null members start here. */
    nsnull,                         // getObjectOps;
    nsnull,                         // checkAccess;
    nsnull,                         // call;
    nsnull,                         // construct;
    nsnull,                         // xdrObject;
    nsnull,                         // hasInstance;
    nsnull,                         // mark;
    nsnull                          // spare;
};

