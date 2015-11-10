// Test ambigious export * statements.

"use strict";

load(libdir + "asserts.js");

function checkModuleEval(source, result) {
    let m = parseModule(source);
    m.declarationInstantiation();
    assertEq(m.evaluation(), result);
}

function checkModuleSyntaxError(source) {
    let m = parseModule(source);
    assertThrowsInstanceOf(() => m.declarationInstantiation(), SyntaxError);
}

let moduleRepo = new Map();
setModuleResolveHook(function(module, specifier) {
    if (specifier in moduleRepo)
        return moduleRepo[specifier];
    throw "Module " + specifier + " not found";
});

let a = moduleRepo['a'] = parseModule("export var a = 1; export var b = 2;");
let b = moduleRepo['b'] = parseModule("export var b = 3; export var c = 4;");
let c = moduleRepo['c'] = parseModule("export * from 'a'; export * from 'b';");
let ms = [a, b, c];
ms.map((m) => m.declarationInstantiation());
ms.map((m) => m.evaluation(), moduleRepo.values());

// Check importing/exporting non-ambiguous name works.
checkModuleEval("import { a } from 'c'; a;", 1);
checkModuleEval("export { a } from 'c';", undefined);

// Check importing/exporting ambiguous name is a syntax error.
checkModuleSyntaxError("import { b } from 'c';");
checkModuleSyntaxError("export { b } from 'c';");

// Check that namespace objects include only non-ambiguous names.
let m = parseModule("import * as ns from 'c'; ns;");
m.declarationInstantiation();
let ns = m.evaluation();
let names = Object.keys(ns);
assertEq(names.length, 2);
assertEq('a' in ns, true);
assertEq('b' in ns, false);
assertEq('c' in ns, true);
