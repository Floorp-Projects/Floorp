// Tests for the assertEqual function in jit-test/lib/asserts.js

load(libdir + "asserts.js");

function assertNotDeepEq(a, b, options) {
    assertThrowsInstanceOf(() => assertDeepEq(a, b, options), Error);
}

// primitives
assertDeepEq(undefined, undefined);
assertDeepEq("1", "1");
assertNotDeepEq(1, "1");
assertNotDeepEq(undefined, null);
assertNotDeepEq({}, null);

// symbols
if (typeof Symbol === "function") {
    assertDeepEq(Symbol(), Symbol());
    assertNotDeepEq(Symbol(), Symbol(""));
    assertDeepEq(Symbol("tweedledum"), Symbol("tweedledum"));
    assertNotDeepEq(Symbol("tweedledum"), Symbol("alice"));
    assertNotDeepEq(Symbol("what-its-called"), Symbol.for("what-its-called"));
    assertNotDeepEq(Symbol.iterator, Symbol.for("Symbol.iterator"));
    assertDeepEq([Symbol(), Symbol(), Symbol()],
                 [Symbol(), Symbol(), Symbol()]);
    var sym = Symbol();
    assertDeepEq([sym, sym], [sym, sym]);
    assertNotDeepEq([sym, sym], [Symbol(), Symbol()]);
    assertNotDeepEq([sym, sym], [Symbol(), sym]);
    var obj1 = {}, obj2 = {};
    obj1[Symbol("x")] = "y";
    obj2[Symbol("x")] = "y";
    assertDeepEq(obj1, obj2);
}

// objects
assertDeepEq({}, {});
assertDeepEq({one: 1, two: 2}, {one: 1, two: 2});
assertNotDeepEq(Object.freeze({}), {});
assertDeepEq(Object.create(null), Object.create(null));
assertNotDeepEq(Object.create(null, {a: {configurable: false, value: 3}}),
               Object.create(null, {a: {configurable: true, value: 3}}));
assertNotDeepEq({one: 1}, {one: 1, two: 2});
assertNotDeepEq({yes: true}, {oui: true});
assertNotDeepEq({zero: 0}, {zero: "0"});

// test the comment
var x = {}, y = {}, ax = [x];
assertDeepEq([ax, x], [ax, y]);  // passes (bogusly)
assertNotDeepEq([ax, x], [ax, y], {strictEquivalence: true});
assertDeepEq([x, ax], [y, ax]);  // passes (bogusly)
assertNotDeepEq([x, ax], [y, ax], {strictEquivalence: true});

// object identity
assertNotDeepEq([x, y], [x, x]);
assertDeepEq([x, y], [x, y]);
assertDeepEq([y, x], [x, y]);

// proto chain
var x = {};
assertDeepEq(Object.create(x), Object.create(x));
assertDeepEq(Object.create({}), Object.create({})); // equivalent but not identical proto objects

// arrays
assertDeepEq([], []);
assertNotDeepEq([], [1]);
assertDeepEq([1], [1]);
assertNotDeepEq([0], [1]);
assertDeepEq([1, 2, 3], [1, 2, 3]);
assertNotDeepEq([1, , 3], [1, undefined, 3]);
var p = [], q = [];
p.prop = 1;
assertNotDeepEq(p, q);
assertNotDeepEq(q, p);
q.prop = 1;
assertDeepEq(q, p);

// functions
assertNotDeepEq(() => 1, () => 2);
assertNotDeepEq((...x) => 1, x => 1);
assertNotDeepEq(function f(){}, function g(){});
var f1 = function () {}, f2 = function () {};
assertDeepEq(f1, f1);
assertDeepEq(f1, f2);  // same text, close enough
f1.prop = 1;
assertNotDeepEq(f1, f2);
f2.prop = 1;
assertDeepEq(f1, f2);

// recursion
var a = [], b = [];
a[0] = a;
b[0] = b;
assertDeepEq(a, b);
a[0] = b;
assertNotDeepEq(a, b);  // [#1=[#1#]] is not structurally equivalent to #1=[[#1#]]
b[0] = a;
assertDeepEq(a, b);
b[0] = [a];  // a[0] === b, b[0] === c, c[0] === a
assertDeepEq(a, b);

// objects that merge
var x = {};
assertDeepEq({x: x}, {x: x});
var y = [x];
assertDeepEq([y], [y]);

// cross-compartment
var g1 = newGlobal(), g2 = newGlobal();
assertDeepEq(g1, g2);
assertDeepEq(g1, g2, {strictEquivalence: true});
Object.preventExtensions(g2.Math.abs);  // make some miniscule change
assertNotDeepEq(g1, g2);
