// This tests that attempting to perform ModuleDeclarationInstantation a second
// time after a failure re-throws the same error.
//
// The first attempt fails becuase module 'a' is not available. The second
// attempt fails because of the previous failure (it would otherwise succeed as
// 'a' is now available).
//
// This test exercises the path where the previously instantiated module is
// encountered as an import.

load(libdir + "dummyModuleResolveHook.js");

let b = moduleRepo['b'] = parseModule("export var b = 3; export var c = 4;");
let c = moduleRepo['c'] = parseModule("export * from 'a'; export * from 'b';");

let e1;
let threw = false;
try {
    c.declarationInstantiation();
} catch (exc) {
    threw = true;
    e1 = exc;
}
assertEq(threw, true);
assertEq(typeof e1 === "undefined", false);

let a = moduleRepo['a'] = parseModule("export var a = 1; export var b = 2;");
let d = moduleRepo['d'] = parseModule("import { a } from 'c'; a;");

threw = false;
let e2;
try {
    d.declarationInstantiation();
} catch (exc) {
    threw = true;
    e2 = exc;
}
assertEq(threw, true);
assertEq(e1, e2);
