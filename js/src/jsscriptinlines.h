/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsscriptinlines_h
#define jsscriptinlines_h

#include "jsscript.h"

#include "jit/BaselineJIT.h"
#include "jit/IonAnalysis.h"
#include "vm/EnvironmentObject.h"
#include "vm/RegExpObject.h"
#include "wasm/AsmJS.h"

#include "jscompartmentinlines.h"

#include "vm/Shape-inl.h"

namespace js {

ScriptCounts::ScriptCounts()
  : pcCounts_(),
    throwCounts_(),
    ionCounts_(nullptr)
{
}

ScriptCounts::ScriptCounts(PCCountsVector&& jumpTargets)
  : pcCounts_(Move(jumpTargets)),
    throwCounts_(),
    ionCounts_(nullptr)
{
}

ScriptCounts::ScriptCounts(ScriptCounts&& src)
  : pcCounts_(Move(src.pcCounts_)),
    throwCounts_(Move(src.throwCounts_)),
    ionCounts_(Move(src.ionCounts_))
{
    src.ionCounts_ = nullptr;
}

ScriptCounts&
ScriptCounts::operator=(ScriptCounts&& src)
{
    pcCounts_ = Move(src.pcCounts_);
    throwCounts_ = Move(src.throwCounts_);
    ionCounts_ = Move(src.ionCounts_);
    src.ionCounts_ = nullptr;
    return *this;
}

ScriptCounts::~ScriptCounts()
{
    js_delete(ionCounts_);
}

ScriptAndCounts::ScriptAndCounts(JSScript* script)
  : script(script),
    scriptCounts()
{
    script->releaseScriptCounts(&scriptCounts);
}

ScriptAndCounts::ScriptAndCounts(ScriptAndCounts&& sac)
  : script(Move(sac.script)),
    scriptCounts(Move(sac.scriptCounts))
{
}

void
SetFrameArgumentsObject(JSContext* cx, AbstractFramePtr frame,
                        HandleScript script, JSObject* argsobj);

/* static */ inline JSFunction*
LazyScript::functionDelazifying(JSContext* cx, Handle<LazyScript*> script)
{
    RootedFunction fun(cx, script->function_);
    if (script->function_ && !JSFunction::getOrCreateScript(cx, fun))
        return nullptr;
    return script->function_;
}

} // namespace js

inline JSFunction*
JSScript::functionDelazifying() const
{
    JSFunction* fun = function();
    if (fun && fun->isInterpretedLazy()) {
        fun->setUnlazifiedScript(const_cast<JSScript*>(this));
        // If this script has a LazyScript, make sure the LazyScript has a
        // reference to the script when delazifying its canonical function.
        if (lazyScript && !lazyScript->maybeScript())
            lazyScript->initScript(const_cast<JSScript*>(this));
    }
    return fun;
}

inline void
JSScript::ensureNonLazyCanonicalFunction()
{
    // Infallibly delazify the canonical script.
    JSFunction* fun = function();
    if (fun && fun->isInterpretedLazy())
        functionDelazifying();
}

inline JSFunction*
JSScript::getFunction(size_t index)
{
    JSFunction* fun = &getObject(index)->as<JSFunction>();
    MOZ_ASSERT_IF(fun->isNative(), IsAsmJSModuleNative(fun->native()));
    return fun;
}

inline js::RegExpObject*
JSScript::getRegExp(size_t index)
{
    return &getObject(index)->as<js::RegExpObject>();
}

inline js::RegExpObject*
JSScript::getRegExp(jsbytecode* pc)
{
    return &getObject(pc)->as<js::RegExpObject>();
}

inline js::GlobalObject&
JSScript::global() const
{
    /*
     * A JSScript always marks its compartment's global (via bindings) so we
     * can assert that maybeGlobal is non-null here.
     */
    return *compartment()->maybeGlobal();
}

inline js::LexicalScope*
JSScript::maybeNamedLambdaScope() const
{
    // Dynamically created Functions via the 'new Function' are considered
    // named lambdas but they do not have the named lambda scope of
    // textually-created named lambdas.
    js::Scope* scope = outermostScope();
    if (scope->kind() == js::ScopeKind::NamedLambda ||
        scope->kind() == js::ScopeKind::StrictNamedLambda)
    {
        MOZ_ASSERT_IF(!strict(), scope->kind() == js::ScopeKind::NamedLambda);
        MOZ_ASSERT_IF(strict(), scope->kind() == js::ScopeKind::StrictNamedLambda);
        return &scope->as<js::LexicalScope>();
    }
    return nullptr;
}

inline js::Shape*
JSScript::initialEnvironmentShape() const
{
    js::Scope* scope = bodyScope();
    if (scope->is<js::FunctionScope>()) {
        if (js::Shape* envShape = scope->environmentShape())
            return envShape;
        if (js::Scope* namedLambdaScope = maybeNamedLambdaScope())
            return namedLambdaScope->environmentShape();
    } else if (scope->is<js::EvalScope>()) {
        return scope->environmentShape();
    }
    return nullptr;
}

inline JSPrincipals*
JSScript::principals()
{
    return compartment()->principals();
}

inline void
JSScript::setBaselineScript(JSRuntime* maybeRuntime, js::jit::BaselineScript* baselineScript)
{
    if (hasBaselineScript())
        js::jit::BaselineScript::writeBarrierPre(zone(), baseline);
    MOZ_ASSERT(!hasIonScript());
    baseline = baselineScript;
    resetWarmUpResetCounter();
    updateBaselineOrIonRaw(maybeRuntime);
}

inline bool
JSScript::ensureHasAnalyzedArgsUsage(JSContext* cx)
{
    if (analyzedArgsUsage())
        return true;
    return js::jit::AnalyzeArgumentsUsage(cx, this);
}

inline bool
JSScript::isDebuggee() const
{
    return compartment_->debuggerObservesAllExecution() || hasDebugScript_;
}

#endif /* jsscriptinlines_h */
