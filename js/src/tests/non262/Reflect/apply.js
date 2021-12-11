/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

// Reflect.apply calls functions.
assertEq(Reflect.apply(Math.floor, undefined, [1.75]), 1);

// Reflect.apply requires a target object that's callable.
var nonCallable = [{}, [], (class clsX { constructor() {} })];
for (var value of nonCallable) {
    assertThrowsInstanceOf(() => Reflect.apply(nonCallable), TypeError);
}

// When target is not callable, Reflect.apply does not try to get argumentList.length before throwing.
var hits = 0;
var bogusArgumentList = {get length() { hit++; throw "FAIL";}};
assertThrowsInstanceOf(() => Reflect.apply({callable: false}, null, bogusArgumentList),
                       TypeError);
assertEq(hits, 0);

// Reflect.apply works on a range of different callable objects.
// Builtin functions (we also tested Math.floor above):
assertEq(Reflect.apply(String.fromCharCode,
                       undefined,
                       [104, 101, 108, 108, 111]),
         "hello");

// Builtin methods:
assertEq(Reflect.apply(RegExp.prototype.exec,
                       /ab/,
                       ["confabulation"]).index,
         4);

// Builtin methods of primitive objects:
assertEq(Reflect.apply("".charAt,
                       "ponies",
                       [3]),
         "i");

// Bound functions:
assertEq(Reflect.apply(function () { return this; }.bind(Math),
                       Function,
                       []),
         Math);
assertEq(Reflect.apply(Array.prototype.concat.bind([1, 2], [3]),
                       [4, 5],
                       [[6, 7, 8]]).join(),
         "1,2,3,6,7,8");

// Generator functions:
function* g(arg) { yield "pass" + arg; }
assertEq(Reflect.apply(g,
                       undefined,
                       ["word"]).next().value,
         "password");

// Proxies:
function f() { return 13; }
assertEq(Reflect.apply(new Proxy(f, {}),
                       undefined,
                       []),
         13);

// Cross-compartment wrappers:
var gw = newGlobal();
assertEq(Reflect.apply(gw.parseInt,
                       undefined,
                       ["45"]),
         45);
assertEq(Reflect.apply(gw.Symbol.for,
                       undefined,
                       ["moon"]),
         Symbol.for("moon"));

gw.eval("function q() { return q; }");
assertEq(Reflect.apply(gw.q,
                       undefined,
                       []),
         gw.q);


// Exceptions are propagated.
var nope = new Error("nope");
function fail() {
    throw nope;
}
assertThrowsValue(() => Reflect.apply(fail, undefined, []),
                  nope);

// Exceptions thrown by cross-compartment wrappers are re-wrapped for the
// calling compartment.
var gxw = gw.eval("var x = new Error('x'); x");
gw.eval("function fail() { throw x; }");
assertThrowsValue(() => Reflect.apply(gw.fail, undefined, []),
                  gxw);

// The thisArgument is passed to the target function as the 'this' value.
var obj = {};
hits = 0;
assertEq(Reflect.apply(function () { hits++; assertEq(this, obj); },
                       obj,
                       []),
         undefined);
assertEq(hits, 1);

// Primitive values can be thisArgument.
function strictThis() { "use strict"; return this; }
for (var value of [null, undefined, 0, -0, NaN, Symbol("moon")]) {
    assertEq(Reflect.apply(strictThis, value, []),
             value);
}

// If the target is a non-strict function and thisArgument is a primitive value
// other than null or undefined, then thisArgument is converted to a wrapper
// object.
var testValues = [true, 1e9, "ok", Symbol("ok")];
function nonStrictThis(expected) {
    assertEq(typeof this, "object");
    assertEq(Reflect.apply(Object.prototype.toString, this, []).toLowerCase(), expected);
    return "ok";
}
for (var value of testValues) {
    assertEq(Reflect.apply(nonStrictThis,
                           value,
                           ["[object " + typeof value + "]"]),
             "ok");
}

// For more Reflect.apply tests, see target.js and argumentsList.js.

reportCompare(0, 0);
