// Test ambigious export * statements.

"use strict";

load(libdir + "asserts.js");

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

let d = moduleRepo['d'] = parseModule("import { a } from 'c'; a;");
d.declarationInstantiation();
assertEq(d.evaluation(), 1);

let e = moduleRepo['e'] = parseModule("import { b } from 'c';");
assertThrowsInstanceOf(() => e.declarationInstantiation(), SyntaxError);
