// This tests that attempting to perform ModuleDeclarationInstantation a second
// time after a failure re-throws the same error.
//
// The first attempt fails becuase module 'a' is not available. The second
// attempt fails because of the previous failure.
//
// This test exercises the path where the previously instantiated module is
// re-instantiated directly.

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

threw = false;
let e2;
try {
    c.declarationInstantiation();
} catch (exc) {
    threw = true;
    e2 = exc;
}
assertEq(threw, true);
assertEq(e1, e2);
