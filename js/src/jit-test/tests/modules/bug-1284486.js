// This tests that module instantiation can succeed when executed a second
// time after a failure.
//
// The first attempt fails becuase module 'a' is not available. The second
// attempt succeeds as 'a' is now available.
//
// This test exercises the path where the previously instantiated module is
// encountered as an import.

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

let a = registerModule('a', parseModule("export var a = 1; export var b = 2;"));
let d = registerModule('d', parseModule("import { a } from 'c'; a;"));

threw = false;
try {
    moduleLink(d);
} catch (exc) {
    threw = true;
}
assertEq(threw, false);
