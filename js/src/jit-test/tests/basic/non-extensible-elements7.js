// Test array.length setter on non-extensible/sealed/frozen arrays.
"use strict";

load(libdir + "asserts.js");

function testNonExtensible() {
    var a = [1, 2, 3, , ,];
    Object.preventExtensions(a);
    for (var i = 0; i < 10; i++)
        a.length = 10;
    assertEq(a.length, 10);
    for (var i = 0; i < 10; i++)
        a.length = 0;
    assertEq(a.length, 0);
    assertEq(a.toString(), "");
}
testNonExtensible();

function testSealed() {
    var a = [1, 2, 3, , ,];
    Object.seal(a);
    for (var i = 0; i < 10; i++)
        a.length = 10;
    assertEq(a.length, 10);
    for (var i = 0; i < 10; i++)
        assertThrowsInstanceOf(() => a.length = 0, TypeError);
    assertEq(a.length, 3);
    assertEq(a.toString(), "1,2,3");
}
testSealed();

function testFrozen() {
    var a = [1, 2, 3, , ,];
    Object.freeze(a);
    for (var i = 0; i < 10; i++)
        assertThrowsInstanceOf(() => a.length = 10, TypeError);
    for (var i = 0; i < 10; i++)
        assertThrowsInstanceOf(() => a.length = 0, TypeError);
    assertEq(a.length, 5);
    assertEq(a.toString(), "1,2,3,,");
}
testFrozen();
