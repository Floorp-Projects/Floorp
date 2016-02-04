/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ScopeObject_inl_h
#define vm_ScopeObject_inl_h

#include "vm/ScopeObject.h"
#include "frontend/SharedContext.h"

#include "jsobjinlines.h"

#include "vm/TypeInference-inl.h"

namespace js {

inline ClonedBlockObject&
NearestEnclosingExtensibleLexicalScope(JSObject* scope)
{
    while (!IsExtensibleLexicalScope(scope))
        scope = scope->enclosingScope();
    return scope->as<ClonedBlockObject>();
}

inline void
ScopeObject::setAliasedVar(JSContext* cx, ScopeCoordinate sc, PropertyName* name, const Value& v)
{
    MOZ_ASSERT(is<LexicalScopeBase>() || is<ClonedBlockObject>());
    JS_STATIC_ASSERT(CallObject::RESERVED_SLOTS == ClonedBlockObject::RESERVED_SLOTS);

    // name may be null if we don't need to track side effects on the object.
    MOZ_ASSERT_IF(isSingleton(), name);

    if (isSingleton()) {
        MOZ_ASSERT(name);
        AddTypePropertyId(cx, this, NameToId(name), v);

        // Keep track of properties which have ever been overwritten.
        if (!getSlot(sc.slot()).isUndefined()) {
            Shape* shape = lookup(cx, name);
            shape->setOverwritten();
        }
    }

    setSlot(sc.slot(), v);
}

inline void
LexicalScopeBase::setAliasedVar(JSContext* cx, AliasedFormalIter fi, PropertyName* name,
                                const Value& v)
{
    MOZ_ASSERT(name == fi->name());
    setSlot(fi.scopeSlot(), v);
    if (isSingleton())
        AddTypePropertyId(cx, this, NameToId(name), v);
}

inline void
LexicalScopeBase::setAliasedVarFromArguments(JSContext* cx, const Value& argsValue, jsid id,
                                             const Value& v)
{
    setSlot(ArgumentsObject::SlotFromMagicScopeSlotValue(argsValue), v);
    if (isSingleton())
        AddTypePropertyId(cx, this, id, v);
}

inline void
LexicalScopeBase::initRemainingSlotsToUninitializedLexicals(uint32_t begin)
{
    uint32_t end = slotSpan();
    for (uint32_t slot = begin; slot < end; slot++)
        initSlot(slot, MagicValue(JS_UNINITIALIZED_LEXICAL));
}

inline void
LexicalScopeBase::initAliasedLexicalsToThrowOnTouch(JSScript* script)
{
    initRemainingSlotsToUninitializedLexicals(script->bindings.aliasedBodyLevelLexicalBegin());
}

template <AllowGC allowGC>
inline void
StaticScopeIter<allowGC>::operator++(int)
{
    if (!scope->template is<StaticFunctionScope>()) {
        scope = scope->enclosingScope();
    } else if (onNamedLambda || !scope->template as<StaticFunctionScope>().isNamedLambda()) {
        onNamedLambda = false;
        scope = scope->enclosingScope();
    } else {
        onNamedLambda = true;
    }
    MOZ_ASSERT_IF(scope, scope->template is<StaticScope>());
    MOZ_ASSERT_IF(onNamedLambda, scope->template is<StaticFunctionScope>());
}

template <AllowGC allowGC>
inline bool
StaticScopeIter<allowGC>::hasSyntacticDynamicScopeObject() const
{
    if (scope->template is<StaticFunctionScope>()) {
        JSFunction& fun = scope->template as<StaticFunctionScope>().function();
        if (fun.isBeingParsed())
            return fun.functionBox()->needsCallObject();
        return fun.needsCallObject();
    }
    if (scope->template is<StaticModuleScope>())
        return true;
    if (scope->template is<StaticBlockScope>()) {
        return scope->template as<StaticBlockScope>().needsClone() ||
               scope->template as<StaticBlockScope>().isGlobal();
    }
    if (scope->template is<StaticWithScope>())
        return true;
    if (scope->template is<StaticEvalScope>())
        return scope->template as<StaticEvalScope>().isStrict();
    MOZ_ASSERT(scope->template is<StaticNonSyntacticScope>());
    return false;
}

template <AllowGC allowGC>
inline Shape*
StaticScopeIter<allowGC>::scopeShape() const
{
    MOZ_ASSERT(hasSyntacticDynamicScopeObject());
    MOZ_ASSERT(type() != NamedLambda && type() != Eval);
    if (type() == Block)
        return block().lastProperty();
    if (type() == Module)
        return module().environmentShape();
    return fun().environmentShape();
}

template <AllowGC allowGC>
inline typename StaticScopeIter<allowGC>::Type
StaticScopeIter<allowGC>::type() const
{
    if (onNamedLambda)
        return NamedLambda;
    if (scope->template is<StaticBlockScope>())
        return Block;
    if (scope->template is<StaticModuleScope>())
        return Module;
    if (scope->template is<StaticWithScope>())
        return With;
    if (scope->template is<StaticEvalScope>())
        return Eval;
    if (scope->template is<StaticNonSyntacticScope>())
        return NonSyntactic;
    MOZ_ASSERT(scope->template is<StaticFunctionScope>());
    return Function;
}

template <AllowGC allowGC>
inline StaticBlockScope&
StaticScopeIter<allowGC>::block() const
{
    MOZ_ASSERT(type() == Block);
    return scope->template as<StaticBlockScope>();
}

template <AllowGC allowGC>
inline StaticWithScope&
StaticScopeIter<allowGC>::staticWith() const
{
    MOZ_ASSERT(type() == With);
    return scope->template as<StaticWithScope>();
}

template <AllowGC allowGC>
inline StaticEvalScope&
StaticScopeIter<allowGC>::eval() const
{
    MOZ_ASSERT(type() == Eval);
    return scope->template as<StaticEvalScope>();
}

template <AllowGC allowGC>
inline StaticNonSyntacticScope&
StaticScopeIter<allowGC>::nonSyntactic() const
{
    MOZ_ASSERT(type() == NonSyntactic);
    return scope->template as<StaticNonSyntacticScope>();
}

template <AllowGC allowGC>
inline JSScript*
StaticScopeIter<allowGC>::funScript() const
{
    MOZ_ASSERT(type() == Function);
    return scope->template as<StaticFunctionScope>().function().nonLazyScript();
}

template <AllowGC allowGC>
inline StaticFunctionScope&
StaticScopeIter<allowGC>::fun() const
{
    MOZ_ASSERT(type() == Function);
    return scope->template as<StaticFunctionScope>();
}

template <AllowGC allowGC>
inline frontend::FunctionBox*
StaticScopeIter<allowGC>::maybeFunctionBox() const
{
    MOZ_ASSERT(type() == Function);
    if (fun().function().isBeingParsed())
        return fun().function().functionBox();
    return nullptr;
}

template <AllowGC allowGC>
inline StaticModuleScope&
StaticScopeIter<allowGC>::module() const
{
    MOZ_ASSERT(type() == Module);
    return scope->template as<StaticModuleScope>();
}

}  /* namespace js */

inline JSObject*
JSObject::enclosingScope()
{
    if (is<js::ScopeObject>())
        return &as<js::ScopeObject>().enclosingScope();

    if (is<js::DebugScopeObject>())
        return &as<js::DebugScopeObject>().enclosingScope();

    if (is<js::GlobalObject>())
        return nullptr;

    MOZ_ASSERT(!is<JSFunction>());
    MOZ_ASSERT(!is<js::StaticScope>());
    return &global();
}

#endif /* vm_ScopeObject_inl_h */
