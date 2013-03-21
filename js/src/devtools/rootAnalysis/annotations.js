/* -*- Mode: Javascript; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

"use strict";

function indirectCallCannotGC(caller, name)
{
    if (name == "mallocSizeOf")
        return true;

    // hook called during script finalization which cannot GC.
    if (/CallDestroyScriptHook/.test(caller))
        return true;

    // hooks called deep inside utility libraries.
    if (name == "_malloc_message")
        return true;

    return false;
}

// classes to ignore indirect calls on.
var ignoreClasses = [
    "JSTracer",
    "JSStringFinalizer",
    "SprintfStateStr",
    "JSLocaleCallbacks",
    "JSC::ExecutableAllocator",
    "_MD_IOVector",
    "PRIOMethods"
];

function fieldCallCannotGC(csu, field)
{
    for (var i = 0; i < ignoreClasses.length; i++) {
        if (csu == ignoreClasses[i])
            return true;
    }
    if (csu == "js::Class" && (field == "trace" || field == "finalize"))
        return true;
    if (csu == "JSRuntime" && field == "destroyPrincipals")
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
            if (/::Unrooted\(\)/.test(name))
                return true;
            if (/::~Unrooted\(\)/.test(name))
                return true;
            if (/~DebugOnly/.test(name))
                return true;
        }
    }

    return false;
}

var ignoreFunctions = [
    "ptio.c:pt_MapError",
    "PR_ExplodeTime",
    "PR_ErrorInstallTable"
];

function ignoreGCFunction(fun)
{
    for (var i = 0; i < ignoreFunctions.length; i++) {
        if (fun == ignoreFunctions[i])
            return true;
    }

    // XXX modify refillFreeList<NoGC> to not need data flow analysis to understand it cannot GC.
    if (/refillFreeList/.test(fun) && /\(js::AllowGC\)0u/.test(fun))
        return true;
    return false;
}

function isRootedTypeName(name)
{
    if (name.startsWith('struct '))
        name = name.substr(7);
    if (name.startsWith('class '))
        name = name.substr(6);
    if (name.startsWith('const '))
        name = name.substr(6);
    if (name.startsWith('js::'))
        name = name.substr(4);
    if (name.startsWith('JS::'))
        name = name.substr(4);

    return name.startsWith('Rooted');
}

function isSuppressConstructor(name)
{
    return /::AutoSuppressGC/.test(name)
        || /::AutoEnterAnalysis/.test(name);
}
