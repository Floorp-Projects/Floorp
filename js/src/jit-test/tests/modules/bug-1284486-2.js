// This tests that attempting to perform ModuleDeclarationInstantation a
// second time after a failure still fails. (It no longer stores and rethrows
// the same error; the spec changed in that regard and the implementation was
// updated in bug 1420420).
//
// The attempts fails becuase module 'a' is not available.
//
// This test exercises the path where the previously instantiated module is
// re-instantiated directly.

let b = registerModule('b', parseModule("export var b = 3; export var c = 4;"));
let c = registerModule('c', parseModule("export * from 'a'; export * from 'b';"));

let e1;
let threw = false;
try {
    moduleLink(c);
} catch (exc) {
    threw = true;
    e1 = exc;
}
assertEq(threw, true);
assertEq(typeof e1 === "undefined", false);

threw = false;
let e2;
try {
    moduleLink(c);
} catch (exc) {
    threw = true;
    e2 = exc;
}
assertEq(threw, true);
assertEq(e1.toString(), e2.toString());
