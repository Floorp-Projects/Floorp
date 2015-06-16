/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

// Tests for the argumentList argument to Reflect.apply and Reflect.construct.

// Reflect.apply and Reflect.construct require an argumentList argument that must be an object.
assertThrowsInstanceOf(() => Reflect.apply(Math.min, undefined),  // missing
                       TypeError);
assertThrowsInstanceOf(() => Reflect.construct(Object),  // missing
                       TypeError);
for (var primitive of SOME_PRIMITIVE_VALUES) {
    assertThrowsInstanceOf(() => Reflect.apply(Math.min, undefined, primitive),
                           TypeError);
    assertThrowsInstanceOf(() => Reflect.construct(Object, primitive),
                           TypeError);
}

// Array used by several tests below.
var BOTH = [
    Reflect.apply,
    // Adapt Reflect.construct to accept the same arguments as Reflect.apply.
    (target, thisArgument, argumentList) => Reflect.construct(target, argumentList)
];

// The argumentList is copied and becomes the list of arguments passed to the function.
function getRest(...x) { return x; }
var args = [1, 2, 3];
for (var method of BOTH) {
    var result = method(getRest, undefined, args);
    assertEq(result.join(), args.join());
    assertEq(result !== args, true);
}

// argumentList.length can be less than func.length.
function testLess(a, b, c, d, e) {
    assertEq(a, 1);
    assertEq(b, true);
    assertEq(c, "three");
    assertEq(d, Symbol.for);
    assertEq(e, undefined);

    assertEq(arguments.length, 4);
    assertEq(arguments !== args, true);
    return "ok";
}
args = [1, true, "three", Symbol.for];
assertEq(Reflect.apply(testLess, undefined, args), "ok");
assertEq(Reflect.construct(testLess, args) instanceof testLess, true);

// argumentList.length can be more than func.length.
function testMoar(a) {
    assertEq(a, args[0]);
    return "good";
}
assertEq(Reflect.apply(testMoar, undefined, args), "good");
assertEq(Reflect.construct(testMoar, args) instanceof testMoar, true);

// argumentList can be any object with a .length property.
function getArgs(...args) {
    return args;
}
for (var method of BOTH) {
    assertDeepEq(method(getArgs, undefined, {length: 0}),
                 []);
    assertDeepEq(method(getArgs, undefined, {length: 1, "0": "zero"}),
                 ["zero"]);
    assertDeepEq(method(getArgs, undefined, {length: 2}),
                 [undefined, undefined]);
    assertDeepEq(method(getArgs, undefined, function (a, b, c) {}),
                 [undefined, undefined, undefined]);
}

// The Iterable/Iterator interfaces are not used.
var funnyArgs = {
    0: "zero",
    1: "one",
    length: 2,
    [Symbol.iterator]() { throw "FAIL 1"; },
    next() { throw "FAIL 2"; }
};
for (var method of BOTH) {
    assertDeepEq(method(getArgs, undefined, funnyArgs),
                 ["zero", "one"]);
}

// If argumentList has no .length property, no arguments are passed.
function count() { return {numArgsReceived: arguments.length}; }
for (var method of BOTH) {
    assertEq(method(count, undefined, {"0": 0, "1": 1}).numArgsReceived,
             0);
    function* g() { yield 1; yield 2; }
    assertEq(method(count, undefined, g()).numArgsReceived,
             0);
}

// If argumentsList.length has a getter, it is called.
var log;
args = {
    get length() { log += "L"; return 1; },
    get "0"() { log += "0"; return "zero"; },
    get "1"() { log += "1"; return "one"; }
};
for (var method of BOTH) {
    log = "";
    assertDeepEq(method(getArgs, undefined, args),
                 ["zero"]);
    assertEq(log, "L0");
}

// The argumentsList.length getter can throw; the exception is propagated.
var exc = {status: "bad"};
args = {
    get length() { throw exc; }
};
for (var method of BOTH) {
    assertThrowsValue(() => method(count, undefined, args), exc);
}

// If argumentsList.length is unreasonably huge, we get an error.
// (This is an implementation limit.)
for (var method of BOTH) {
    for (var value of [1e12, 1e240, Infinity]) {
        assertThrowsInstanceOf(() => method(count, undefined, {length: value}),
                               Error);
    }
}

// argumentsList.length is converted to an integer.
for (var value of [1.7, "1", {valueOf() { return "1"; }}]) {
    args = {
        length: value,
        "0": "ponies"
    };
    for (var method of BOTH) {
        var result = method(getArgs, undefined, args);
        assertEq(result.length, 1);
        assertEq(result[0], "ponies");
    }
}

// If argumentsList.length is negative or NaN, no arguments are passed.
for (var method of BOTH) {
    for (var num of [-1, -0.1, -0, -1e99, -Infinity, NaN]) {
        assertEq(method(count, undefined, {length: num}).numArgsReceived,
                 0);
    }
}

// Many arguments can be passed.
var many = 65537;
var args = {length: many, 0: "zero", [many - 1]: "last"};
function testMany(...args) {
    "use strict";
    for (var i = 0; i < many; i++) {
        assertEq(i in args, true);
        assertEq(args[i], i === 0 ? "zero" : i === many - 1 ? "last" : undefined);
    }
    return this;
}
assertEq(Reflect.apply(testMany, "pass", args), "pass");
assertEq(Reflect.construct(testMany, args) instanceof testMany, true);

reportCompare(0, 0);
