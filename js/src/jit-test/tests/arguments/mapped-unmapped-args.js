// An unmapped arguments object is created for strict functions or functions
// with default/rest/destructuring args.

load(libdir + "asserts.js");

function testDefaults(a, b=3) {
    a = 3;
    b = 4;
    assertEq(arguments.length, 1);
    assertEq(arguments[0], 1);
    assertEq(arguments[1], undefined);
    arguments[0] = 5;
    assertEq(a, 3);
    assertThrowsInstanceOf(() => arguments.callee, TypeError);
}
testDefaults(1);

function testRest(a, ...rest) {
    a = 3;
    assertEq(arguments.length, 3);
    assertEq(arguments[0], 1);
    assertEq(arguments[1], 2);
    arguments[0] = 5;
    assertEq(a, 3);
    arguments[1] = 6;
    assertEq(arguments[1], 6);
    assertEq(rest.toString(), "2,3");
    assertThrowsInstanceOf(() => arguments.callee, TypeError);
}
testRest(1, 2, 3);

function testDestructuring(a, {foo, bar}, b) {
    a = 3;
    bar = 4;
    b = 1;
    assertEq(arguments.length, 3);
    assertEq(arguments[0], 1);
    assertEq(arguments[1].bar, 2);
    assertEq(arguments[2], 9);
    assertThrowsInstanceOf(() => arguments.callee, TypeError);
}
testDestructuring(1, {foo: 1, bar: 2}, 9);

function testStrict(a) {
    "use strict";
    a = 3;
    assertEq(arguments[0], 1);
    arguments[0] = 8;
    assertEq(a, 3);
    assertThrowsInstanceOf(() => arguments.callee, TypeError);
}
testStrict(1, 2);

function testMapped(a) {
    a = 3;
    assertEq(arguments[0], 3);
    arguments[0] = 5;
    assertEq(a, 5);
    assertEq(arguments.callee, testMapped);
}
testMapped(1);
