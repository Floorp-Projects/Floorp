/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsfuninlines_h
#define jsfuninlines_h

#include "jsfun.h"

#include "vm/ScopeObject.h"

inline void
JSFunction::initAtom(JSAtom *atom)
{
    atom_.init(atom);
}

inline void
JSFunction::setGuessedAtom(JSAtom *atom)
{
    JS_ASSERT(atom_ == NULL);
    JS_ASSERT(atom != NULL);
    JS_ASSERT(!hasGuessedAtom());
    atom_ = atom;
    flags |= HAS_GUESSED_ATOM;
}

inline void
JSFunction::setEnvironment(JSObject *obj)
{
    JS_ASSERT(isInterpreted());
    *(js::HeapPtrObject *)&u.i.env_ = obj;
}

inline void
JSFunction::initEnvironment(JSObject *obj)
{
    JS_ASSERT(isInterpreted());
    ((js::HeapPtrObject *)&u.i.env_)->init(obj);
}

inline void
JSFunction::initializeExtended()
{
    JS_ASSERT(isExtended());

    JS_ASSERT(mozilla::ArrayLength(toExtended()->extendedSlots) == 2);
    toExtended()->extendedSlots[0].init(js::UndefinedValue());
    toExtended()->extendedSlots[1].init(js::UndefinedValue());
}

inline void
JSFunction::initExtendedSlot(size_t which, const js::Value &val)
{
    JS_ASSERT(which < mozilla::ArrayLength(toExtended()->extendedSlots));
    toExtended()->extendedSlots[which].init(val);
}

inline void
JSFunction::setExtendedSlot(size_t which, const js::Value &val)
{
    JS_ASSERT(which < mozilla::ArrayLength(toExtended()->extendedSlots));
    toExtended()->extendedSlots[which] = val;
}

namespace js {

inline const char *
GetFunctionNameBytes(JSContext *cx, JSFunction *fun, JSAutoByteString *bytes)
{
    JSAtom *atom = fun->atom();
    if (atom)
        return bytes->encodeLatin1(cx, atom);
    return js_anonymous_str;
}

static inline JSObject *
SkipScopeParent(JSObject *parent)
{
    if (!parent)
        return NULL;
    while (parent->is<ScopeObject>())
        parent = &parent->as<ScopeObject>().enclosingScope();
    return parent;
}

inline bool
CanReuseFunctionForClone(JSContext *cx, HandleFunction fun)
{
    if (!fun->hasSingletonType())
        return false;
    if (fun->isInterpretedLazy()) {
        LazyScript *lazy = fun->lazyScript();
        if (lazy->hasBeenCloned())
            return false;
        lazy->setHasBeenCloned();
    } else {
        JSScript *script = fun->nonLazyScript();
        if (script->hasBeenCloned)
            return false;
        script->hasBeenCloned = true;
    }
    return true;
}

inline JSFunction *
CloneFunctionObjectIfNotSingleton(JSContext *cx, HandleFunction fun, HandleObject parent,
                                  NewObjectKind newKind = GenericObject)
{
    /*
     * For attempts to clone functions at a function definition opcode,
     * try to avoid the the clone if the function has singleton type. This
     * was called pessimistically, and we need to preserve the type's
     * property that if it is singleton there is only a single object
     * with its type in existence.
     *
     * For functions inner to run once lambda, it may be possible that
     * the lambda runs multiple times and we repeatedly clone it. In these
     * cases, fall through to CloneFunctionObject, which will deep clone
     * the function's script.
     */
    if (CanReuseFunctionForClone(cx, fun)) {
        RootedObject obj(cx, SkipScopeParent(parent));
        if (!JSObject::setParent(cx, fun, obj))
            return NULL;
        fun->setEnvironment(parent);
        return fun;
    }

    // These intermediate variables are needed to avoid link errors on some
    // platforms.  Sigh.
    gc::AllocKind finalizeKind = JSFunction::FinalizeKind;
    gc::AllocKind extendedFinalizeKind = JSFunction::ExtendedFinalizeKind;
    gc::AllocKind kind = fun->isExtended()
                         ? extendedFinalizeKind
                         : finalizeKind;
    return CloneFunctionObject(cx, fun, parent, kind, newKind);
}

} /* namespace js */

inline JSScript *
JSFunction::existingScript()
{
    JS_ASSERT(isInterpreted());
    if (isInterpretedLazy()) {
        js::LazyScript *lazy = lazyScript();
        JSScript *script = lazy->maybeScript();
        JS_ASSERT(script);

        if (zone()->needsBarrier())
            js::LazyScript::writeBarrierPre(lazy);

        flags &= ~INTERPRETED_LAZY;
        flags |= INTERPRETED;
        initScript(script);
    }
    JS_ASSERT(hasScript());
    return u.i.s.script_;
}

inline void
JSFunction::setScript(JSScript *script_)
{
    JS_ASSERT(isInterpreted());
    mutableScript() = script_;
}

inline void
JSFunction::initScript(JSScript *script_)
{
    JS_ASSERT(isInterpreted());
    mutableScript().init(script_);
}

inline JSObject *
JSFunction::getBoundFunctionTarget() const
{
    JS_ASSERT(isBoundFunction());

    /* Bound functions abuse |parent| to store their target function. */
    return getParent();
}

#endif /* jsfuninlines_h */
