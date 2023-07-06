// |jit-test| skip-if: !('Function' in WebAssembly)

load(libdir + "asserts.js");

function testWasmFunc(f, arg, rval) {
    var func = new WebAssembly.Function({parameters: ["i32"], results: ["i32"]}, f);
    assertEq(func instanceof WebAssembly.Function, true);
    assertEq(func(arg), rval);
}

let f = n => n + 5;

// Callable objects.
testWasmFunc(f, 1, 6);
testWasmFunc(f.bind(null), 2, 7);
testWasmFunc(f.bind(null, 9), 2, 14);
testWasmFunc(new Proxy(f, {}), 3, 8);
testWasmFunc(new Proxy(f, {apply() { return 123; }}), 3, 123);

// Cross-realm but same-compartment function.
let global1 = newGlobal({sameCompartmentAs: this});
global1.evaluate("function f(n) { return n + 8; }");
testWasmFunc(global1.f, 1, 9);

// Non-callable values.
assertErrorMessage(() => testWasmFunc({}, 1, 6), TypeError, /must be a function/);
assertErrorMessage(() => testWasmFunc(null, 1, 6), TypeError, /must be a function/);
assertErrorMessage(() => testWasmFunc("", 1, 6), TypeError, /must be a function/);

// Cross-compartment wrappers are not supported yet. See IsCallableNonCCW.
let global2 = newGlobal({newCompartment: true});
global2.evaluate("function f() {}");
assertErrorMessage(() => testWasmFunc(global2.f, 1, 6), TypeError, /must be a function/);
