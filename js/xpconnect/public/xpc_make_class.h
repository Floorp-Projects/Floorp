/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef xpc_make_class_h
#define xpc_make_class_h

// This file should be used to create JSClass instances for nsIXPCScriptable
// instances. This includes any file that uses xpc_map_end.h.

#include "xpcpublic.h"
#include "mozilla/dom/DOMJSClass.h"

bool XPC_WN_MaybeResolvingPropertyStub(JSContext* cx, JS::HandleObject obj,
                                       JS::HandleId id, JS::HandleValue v);
bool XPC_WN_CannotModifyPropertyStub(JSContext* cx, JS::HandleObject obj,
                                     JS::HandleId id, JS::HandleValue v);

bool XPC_WN_MaybeResolvingDeletePropertyStub(JSContext* cx,
                                             JS::HandleObject obj,
                                             JS::HandleId id,
                                             JS::ObjectOpResult& result);
bool XPC_WN_CannotDeletePropertyStub(JSContext* cx, JS::HandleObject obj,
                                     JS::HandleId id,
                                     JS::ObjectOpResult& result);

bool XPC_WN_Shared_Enumerate(JSContext* cx, JS::HandleObject obj);

bool XPC_WN_NewEnumerate(JSContext* cx, JS::HandleObject obj,
                         JS::MutableHandleIdVector properties,
                         bool enumerableOnly);

bool XPC_WN_Helper_Resolve(JSContext* cx, JS::HandleObject obj, JS::HandleId id,
                           bool* resolvedp);

void XPC_WN_Helper_Finalize(JSFreeOp* fop, JSObject* obj);
void XPC_WN_NoHelper_Finalize(JSFreeOp* fop, JSObject* obj);

bool XPC_WN_Helper_Call(JSContext* cx, unsigned argc, JS::Value* vp);

bool XPC_WN_Helper_HasInstance(JSContext* cx, JS::HandleObject obj,
                               JS::MutableHandleValue valp, bool* bp);

bool XPC_WN_Helper_Construct(JSContext* cx, unsigned argc, JS::Value* vp);

void XPCWrappedNative_Trace(JSTracer* trc, JSObject* obj);

extern const js::ClassExtension XPC_WN_JSClassExtension;

#define XPC_MAKE_CLASS_OPS(_flags)                                             \
  {                                                                            \
    /* addProperty */                                                          \
    ((_flags)&XPC_SCRIPTABLE_USE_JSSTUB_FOR_ADDPROPERTY) ? nullptr             \
    : ((_flags)&XPC_SCRIPTABLE_ALLOW_PROP_MODS_DURING_RESOLVE)                 \
        ? XPC_WN_MaybeResolvingPropertyStub                                    \
        : XPC_WN_CannotModifyPropertyStub,                                     \
                                                                               \
        /* delProperty */                                                      \
        ((_flags)&XPC_SCRIPTABLE_USE_JSSTUB_FOR_DELPROPERTY) ? nullptr         \
        : ((_flags)&XPC_SCRIPTABLE_ALLOW_PROP_MODS_DURING_RESOLVE)             \
            ? XPC_WN_MaybeResolvingDeletePropertyStub                          \
            : XPC_WN_CannotDeletePropertyStub,                                 \
                                                                               \
        /* enumerate */                                                        \
        ((_flags)&XPC_SCRIPTABLE_WANT_NEWENUMERATE)                            \
            ? nullptr /* We will use newEnumerate set below in this case */    \
            : XPC_WN_Shared_Enumerate,                                         \
                                                                               \
        /* newEnumerate */                                                     \
        ((_flags)&XPC_SCRIPTABLE_WANT_NEWENUMERATE) ? XPC_WN_NewEnumerate      \
                                                    : nullptr,                 \
                                                                               \
        /* resolve */ /* We have to figure out resolve strategy at call time   \
                       */                                                      \
        XPC_WN_Helper_Resolve,                                                 \
                                                                               \
        /* mayResolve */                                                       \
        nullptr,                                                               \
                                                                               \
        /* finalize */                                                         \
        ((_flags)&XPC_SCRIPTABLE_WANT_FINALIZE) ? XPC_WN_Helper_Finalize       \
                                                : XPC_WN_NoHelper_Finalize,    \
                                                                               \
        /* call */                                                             \
        ((_flags)&XPC_SCRIPTABLE_WANT_CALL) ? XPC_WN_Helper_Call : nullptr,    \
                                                                               \
        /* hasInstance */                                                      \
        ((_flags)&XPC_SCRIPTABLE_WANT_HASINSTANCE) ? XPC_WN_Helper_HasInstance \
                                                   : nullptr,                  \
                                                                               \
        /* construct */                                                        \
        ((_flags)&XPC_SCRIPTABLE_WANT_CONSTRUCT) ? XPC_WN_Helper_Construct     \
                                                 : nullptr,                    \
                                                                               \
        /* trace */                                                            \
        ((_flags)&XPC_SCRIPTABLE_IS_GLOBAL_OBJECT) ? JS_GlobalObjectTraceHook  \
                                                   : XPCWrappedNative_Trace,   \
  }

#define XPC_MAKE_CLASS(_name, _flags, _classOps)                   \
  {                                                                \
    /* name */                                                     \
    _name,                                                         \
                                                                   \
        /* flags */                                                \
        JSCLASS_SLOT0_IS_NSISUPPORTS | JSCLASS_IS_WRAPPED_NATIVE | \
            JSCLASS_FOREGROUND_FINALIZE |                          \
            (((_flags)&XPC_SCRIPTABLE_IS_GLOBAL_OBJECT)            \
                 ? XPCONNECT_GLOBAL_FLAGS                          \
                 : JSCLASS_HAS_RESERVED_SLOTS(1)),                 \
                                                                   \
        /* cOps */                                                 \
        _classOps,                                                 \
                                                                   \
        /* spec */                                                 \
        nullptr,                                                   \
                                                                   \
        /* ext */                                                  \
        &XPC_WN_JSClassExtension,                                  \
                                                                   \
        /* oOps */                                                 \
        nullptr,                                                   \
  }

#endif
