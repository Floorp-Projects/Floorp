/* -*- Mode: Javascript; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

"use strict";

// Ignore calls made through these function pointers
var ignoreIndirectCalls = {
    "mallocSizeOf" : true,
    "aMallocSizeOf" : true,
    "_malloc_message" : true,
    "__conv" : true,
    "__convf" : true,
    "prerrortable.c:callback_newtable" : true,
    "mozalloc_oom.cpp:void (* gAbortHandler)(size_t)" : true,

    // I don't know why these are getting truncated
    "nsTraceRefcntImpl.cpp:void (* leakyLogAddRef)(void*": true,
    "nsTraceRefcntImpl.cpp:void (* leakyLogAddRef)(void*, int, int)": true,
    "nsTraceRefcntImpl.cpp:void (* leakyLogRelease)(void*": true,
    "nsTraceRefcntImpl.cpp:void (* leakyLogRelease)(void*, int, int)": true,
};

function indirectCallCannotGC(caller, name)
{
    if (name in ignoreIndirectCalls)
        return true;

    if (name == "mapper" && caller == "ptio.c:pt_MapError")
        return true;

    if (name == "params" && caller == "PR_ExplodeTime")
        return true;

    if (name == "op" && /GetWeakmapKeyDelegate/.test(caller))
        return true;

    var CheckCallArgs = "AsmJS.cpp:uint8 CheckCallArgs(FunctionCompiler*, js::frontend::ParseNode*, (uint8)(FunctionCompiler*,js::frontend::ParseNode*,Type)*, FunctionCompiler::Call*)";
    if (name == "checkArg" && caller == CheckCallArgs)
        return true;

    // hook called during script finalization which cannot GC.
    if (/CallDestroyScriptHook/.test(caller))
        return true;

    // template method called during marking and hence cannot GC
    if (name == "op" &&
        /^bool js::WeakMap<Key, Value, HashPolicy>::keyNeedsMark\(JSObject\*\)/.test(caller))
    {
        return true;
    }

    return false;
}

// Ignore calls through functions pointers with these types
var ignoreClasses = {
    "JSTracer" : true,
    "JSStringFinalizer" : true,
    "SprintfStateStr" : true,
    "JSLocaleCallbacks" : true,
    "JSC::ExecutableAllocator" : true,
    "PRIOMethods": true,
    "XPCOMFunctions" : true, // I'm a little unsure of this one
    "_MD_IOVector" : true,
};

// Ignore calls through TYPE.FIELD, where TYPE is the class or struct name containing
// a function pointer field named FIELD.
var ignoreCallees = {
    "js::Class.trace" : true,
    "js::Class.finalize" : true,
    "JSRuntime.destroyPrincipals" : true,
    "nsIGlobalObject.GetGlobalJSObject" : true, // virtual but no implementation can GC
    "nsAXPCNativeCallContext.GetJSContext" : true,
    "js::jit::MDefinition.op" : true, // macro generated virtuals just return a constant
    "js::jit::MDefinition.opName" : true, // macro generated virtuals just return a constant
    "js::jit::LInstruction.getDef" : true, // virtual but no implementation can GC
    "js::jit::IonCache.kind" : true, // macro generated virtuals just return a constant
    "icu_50::UObject.__deleting_dtor" : true, // destructors in ICU code can't cause GC
    "mozilla::CycleCollectedJSRuntime.DescribeCustomObjects" : true, // During tracing, cannot GC.
    "mozilla::CycleCollectedJSRuntime.NoteCustomGCThingXPCOMChildren" : true, // During tracing, cannot GC.
    "nsIThreadManager.GetIsMainThread" : true,
    "PLDHashTableOps.hashKey" : true,
};

function fieldCallCannotGC(csu, fullfield)
{
    if (csu in ignoreClasses)
        return true;
    if (fullfield in ignoreCallees)
        return true;
    return false;
}

function shouldSuppressGC(name)
{
    // Various dead code that should only be called inside AutoEnterAnalysis.
    // Functions with no known caller are by default treated as not suppressing GC.
    return /TypeScript::Purge/.test(name)
        || /StackTypeSet::addPropagateThis/.test(name)
        || /ScriptAnalysis::addPushedType/.test(name)
        || /IonBuilder/.test(name);
}

function ignoreEdgeUse(edge, variable)
{
    // Functions which should not be treated as using variable.
    if (edge.Kind == "Call") {
        var callee = edge.Exp[0];
        if (callee.Kind == "Var") {
            var name = callee.Variable.Name[0];
            if (/~Anchor/.test(name))
                return true;
            if (/~DebugOnly/.test(name))
                return true;
            if (/~ScopedThreadSafeStringInspector/.test(name))
                return true;
        }
    }

    return false;
}

function ignoreEdgeAddressTaken(edge)
{
    // Functions which may take indirect pointers to unrooted GC things,
    // but will copy them into rooted locations before calling anything
    // that can GC. These parameters should usually be replaced with
    // handles or mutable handles.
    if (edge.Kind == "Call") {
        var callee = edge.Exp[0];
        if (callee.Kind == "Var") {
            var name = callee.Variable.Name[0];
            if (/js::Invoke\(/.test(name))
                return true;
        }
    }

    return false;
}

// Ignore calls of these functions (so ignore any stack containing these)
var ignoreFunctions = {
    "ptio.c:pt_MapError" : true,
    "PR_ExplodeTime" : true,
    "PR_ErrorInstallTable" : true,
    "PR_SetThreadPrivate" : true,
    "JSObject* js::GetWeakmapKeyDelegate(JSObject*)" : true, // FIXME: mark with AutoAssertNoGC instead
    "uint8 NS_IsMainThread()" : true,

    // FIXME!
    "NS_LogInit": true,
    "NS_LogTerm": true,
    "NS_LogAddRef": true,
    "NS_LogRelease": true,
    "NS_LogCtor": true,
    "NS_LogDtor": true,
    "NS_LogCOMPtrAddRef": true,
    "NS_LogCOMPtrRelease": true,

    // FIXME!
    "NS_DebugBreak": true,

    // Bug 940765 - fetching preferences should not GC
    "PrefHashEntry* pref_HashTableLookup(void*)": true,
    "uint8 mozilla::Preferences::InitStaticMembers()": true, // Temporary, see bug 940765

    // These are a little overzealous -- these destructors *can* GC if they end
    // up wrapping a pending exception. See bug 898815 for the heavyweight fix.
    "void js::AutoCompartment::~AutoCompartment(int32)" : true,
    "void JSAutoCompartment::~JSAutoCompartment(int32)" : true,

    // And these are workarounds to avoid even more analysis work,
    // which would sadly still be needed even with bug 898815.
    "void js::AutoCompartment::AutoCompartment(js::ExclusiveContext*, JSCompartment*)": true,
};

function ignoreGCFunction(fun)
{
    if (fun in ignoreFunctions)
        return true;

    // Templatized function
    if (fun.indexOf("void nsCOMPtr<T>::Assert_NoQueryNeeded()") >= 0)
        return true;

    // XXX modify refillFreeList<NoGC> to not need data flow analysis to understand it cannot GC.
    if (/refillFreeList/.test(fun) && /\(js::AllowGC\)0u/.test(fun))
        return true;
    return false;
}

function isRootedTypeName(name)
{
    if (name == "mozilla::ErrorResult" ||
        name == "JSErrorResult" ||
        name == "WrappableJSErrorResult" ||
        name == "js::frontend::TokenStream" ||
        name == "js::frontend::TokenStream::Position" ||
        name == "ModuleCompiler")
    {
        return true;
    }
    return false;
}

function isRootedPointerTypeName(name)
{
    if (name.startsWith('struct '))
        name = name.substr(7);
    if (name.startsWith('class '))
        name = name.substr(6);
    if (name.startsWith('const '))
        name = name.substr(6);
    if (name.startsWith('js::ctypes::'))
        name = name.substr(12);
    if (name.startsWith('js::'))
        name = name.substr(4);
    if (name.startsWith('JS::'))
        name = name.substr(4);
    if (name.startsWith('mozilla::dom::'))
        name = name.substr(14);

    if (name.startsWith('MaybeRooted<'))
        return /\(js::AllowGC\)1u>::RootType/.test(name);

    return name.startsWith('Rooted');
}

function isSuppressConstructor(name)
{
    return /::AutoSuppressGC/.test(name)
        || /::AutoEnterAnalysis/.test(name)
        || /::AutoAssertNoGC/.test(name);
}
