/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
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
 * The Original Code is Mozilla XPConnect  code, released
 * June 30, 2009.
 *
 * The Initial Developer of the Original Code is
 *    The Mozilla Foundation
 *
 * Contributor(s):
 *    Andreas Gal <gal@mozilla.com>
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

#ifndef xpcpublic_h
#define xpcpublic_h

#include "jsapi.h"
#include "js/MemoryMetrics.h"
#include "jsclass.h"
#include "jsfriendapi.h"
#include "jsgc.h"
#include "jspubtd.h"
#include "jsproxy.h"

#include "nsISupports.h"
#include "nsIPrincipal.h"
#include "nsWrapperCache.h"
#include "nsStringGlue.h"
#include "nsTArray.h"

class nsIPrincipal;
class nsIXPConnectWrappedJS;
struct nsDOMClassInfoData;

#ifndef BAD_TLS_INDEX
#define BAD_TLS_INDEX ((PRUint32) -1)
#endif

nsresult
xpc_CreateGlobalObject(JSContext *cx, JSClass *clasp,
                       nsIPrincipal *principal, nsISupports *ptr,
                       bool wantXrays, JSObject **global,
                       JSCompartment **compartment);

nsresult
xpc_CreateMTGlobalObject(JSContext *cx, JSClass *clasp,
                         nsISupports *ptr, JSObject **global,
                         JSCompartment **compartment);

#define XPCONNECT_GLOBAL_FLAGS                                                \
    JSCLASS_XPCONNECT_GLOBAL | JSCLASS_HAS_PRIVATE |                          \
    JSCLASS_PRIVATE_IS_NSISUPPORTS | JSCLASS_IMPLEMENTS_BARRIERS |            \
    JSCLASS_GLOBAL_FLAGS_WITH_SLOTS(1)

void
TraceXPCGlobal(JSTracer *trc, JSObject *obj);

// XXX where should this live?
NS_EXPORT_(void)
xpc_LocalizeContext(JSContext *cx);

nsresult
xpc_MorphSlimWrapper(JSContext *cx, nsISupports *tomorph);

static inline bool IS_WRAPPER_CLASS(js::Class* clazz)
{
    return clazz->ext.isWrappedNative;
}

inline JSBool
DebugCheckWrapperClass(JSObject* obj)
{
    NS_ASSERTION(IS_WRAPPER_CLASS(js::GetObjectClass(obj)),
                 "Forgot to check if this is a wrapper?");
    return true;
}

// If IS_WRAPPER_CLASS for the JSClass of an object is true, the object can be
// a slim wrapper, holding a native in its private slot, or a wrappednative
// wrapper, holding the XPCWrappedNative in its private slot. A slim wrapper
// also holds a pointer to its XPCWrappedNativeProto in a reserved slot, we can
// check that slot for a non-void value to distinguish between the two.

// Only use these macros if IS_WRAPPER_CLASS(GetObjectClass(obj)) is true.
#define IS_WN_WRAPPER_OBJECT(obj)                                             \
    (DebugCheckWrapperClass(obj) && js::GetReservedSlot(obj, 0).isUndefined())
#define IS_SLIM_WRAPPER_OBJECT(obj)                                           \
    (DebugCheckWrapperClass(obj) && !js::GetReservedSlot(obj, 0).isUndefined())

// Use these macros if IS_WRAPPER_CLASS(GetObjectClass(obj)) might be false.
// Avoid calling them if IS_WRAPPER_CLASS(GetObjectClass(obj)) can only be
// true, as we'd do a redundant call to IS_WRAPPER_CLASS.
#define IS_WN_WRAPPER(obj)                                                    \
    (IS_WRAPPER_CLASS(js::GetObjectClass(obj)) && IS_WN_WRAPPER_OBJECT(obj))
#define IS_SLIM_WRAPPER(obj)                                                  \
    (IS_WRAPPER_CLASS(js::GetObjectClass(obj)) && IS_SLIM_WRAPPER_OBJECT(obj))

inline JSObject *
xpc_GetGlobalForObject(JSObject *obj)
{
    while (JSObject *parent = js::GetObjectParent(obj))
        obj = parent;
    return obj;
}

extern bool
xpc_OkToHandOutWrapper(nsWrapperCache *cache);

inline JSObject*
xpc_FastGetCachedWrapper(nsWrapperCache *cache, JSObject *scope, jsval *vp)
{
    if (cache) {
        JSObject* wrapper = cache->GetWrapper();
        NS_ASSERTION(!wrapper ||
                     !cache->IsProxy() ||
                     !IS_SLIM_WRAPPER(wrapper),
                     "Should never have a slim wrapper when IsProxy()");
        if (wrapper &&
            js::GetObjectCompartment(wrapper) == js::GetObjectCompartment(scope) &&
            (IS_SLIM_WRAPPER(wrapper) ||
             xpc_OkToHandOutWrapper(cache))) {
            *vp = OBJECT_TO_JSVAL(wrapper);
            return wrapper;
        }
    }

    return nsnull;
}

inline JSObject*
xpc_FastGetCachedWrapper(nsWrapperCache *cache, JSObject *scope)
{
    jsval dummy;
    return xpc_FastGetCachedWrapper(cache, scope, &dummy);
}

// The JS GC marks objects gray that are held alive directly or
// indirectly by an XPConnect root. The cycle collector explores only
// this subset of the JS heap.
inline JSBool
xpc_IsGrayGCThing(void *thing)
{
    return js::GCThingIsMarkedGray(thing);
}

// The cycle collector only cares about some kinds of GCthings that are
// reachable from an XPConnect root. Implemented in nsXPConnect.cpp.
extern JSBool
xpc_GCThingIsGrayCCThing(void *thing);

// Implemented in nsXPConnect.cpp.
extern void
xpc_UnmarkGrayObjectRecursive(JSObject* obj);

// Remove the gray color from the given JSObject and any other objects that can
// be reached through it.
inline void
xpc_UnmarkGrayObject(JSObject *obj)
{
    if (obj) {
        if (xpc_IsGrayGCThing(obj))
            xpc_UnmarkGrayObjectRecursive(obj);
        else if (js::IsIncrementalBarrierNeededOnObject(obj))
            js::IncrementalReferenceBarrier(obj);
    }
}

// If aVariant is an XPCVariant, this marks the object to be in aGeneration.
// This also unmarks the gray JSObject.
extern void
xpc_MarkInCCGeneration(nsISupports* aVariant, PRUint32 aGeneration);

// Unmarks aWrappedJS's JSObject.
extern void
xpc_UnmarkGrayObject(nsIXPConnectWrappedJS* aWrappedJS);

extern void
xpc_UnmarkSkippableJSHolders();

// No JS can be on the stack when this is called. Probably only useful from
// xpcshell.
NS_EXPORT_(void)
xpc_ActivateDebugMode();

namespace xpc {

// If these functions return false, then an exception will be set on cx.
bool Base64Encode(JSContext *cx, JS::Value val, JS::Value *out);
bool Base64Decode(JSContext *cx, JS::Value val, JS::Value *out);

/**
 * Convert an nsString to jsval, returning true on success.
 * Note, the ownership of the string buffer may be moved from str to rval.
 * If that happens, str will point to an empty string after this call.
 */
bool StringToJsval(JSContext *cx, nsAString &str, JS::Value *rval);
bool NonVoidStringToJsval(JSContext *cx, nsAString &str, JS::Value *rval);

void *GetCompartmentName(JSRuntime *rt, JSCompartment *c);
void DestroyCompartmentName(void *string);

#ifdef DEBUG
void DumpJSHeap(FILE* file);
#endif
} // namespace xpc

class nsIMemoryMultiReporterCallback;

namespace mozilla {
namespace xpconnect {
namespace memory {

// This reports all the stats in |rtStats| that belong in the "explicit" tree,
// (which isn't all of them).
nsresult
ReportJSRuntimeExplicitTreeStats(const JS::RuntimeStats &rtStats,
                                 const nsACString &pathPrefix,
                                 nsIMemoryMultiReporterCallback *cb,
                                 nsISupports *closure);

} // namespace memory
} // namespace xpconnect

namespace dom {
namespace binding {

extern int HandlerFamily;
inline void* ProxyFamily() { return &HandlerFamily; }
inline bool instanceIsProxy(JSObject *obj)
{
    return js::IsProxy(obj) &&
           js::GetProxyHandler(obj)->family() == ProxyFamily();
}
extern JSClass ExpandoClass;
inline bool isExpandoObject(JSObject *obj)
{
    return js::GetObjectJSClass(obj) == &ExpandoClass;
}

enum {
    JSPROXYSLOT_PROTOSHAPE = 0,
    JSPROXYSLOT_EXPANDO = 1
};

typedef JSObject*
(*DefineInterface)(JSContext *cx, XPCWrappedNativeScope *scope, bool *enabled);

extern bool
DefineStaticJSVals(JSContext *cx);
void
Register(nsDOMClassInfoData *aData);
extern bool
DefineConstructor(JSContext *cx, JSObject *obj, DefineInterface aDefine,
                  nsresult *aResult);

} // namespace binding
} // namespace dom
} // namespace mozilla

#endif
