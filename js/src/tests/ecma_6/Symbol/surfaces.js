/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

if (typeof Symbol === "function") {
    // Test superficial properties of the Symbol constructor and prototype.

    var desc = Object.getOwnPropertyDescriptor(this, "Symbol");
    assertEq(desc.configurable, true);
    assertEq(desc.enumerable, false);
    assertEq(desc.writable, true);
    assertEq(typeof Symbol, "function");
    assertEq(Symbol.length, 1);

    desc = Object.getOwnPropertyDescriptor(Symbol, "prototype");
    assertEq(desc.configurable, false);
    assertEq(desc.enumerable, false);
    assertEq(desc.writable, false);

    assertEq(Symbol.prototype.constructor, Symbol);
    desc = Object.getOwnPropertyDescriptor(Symbol.prototype, "constructor");
    assertEq(desc.configurable, true);
    assertEq(desc.enumerable, false);
    assertEq(desc.writable, true);

    assertEq(Symbol.for.length, 1);
    assertEq(Symbol.prototype.toString.length, 0);
    assertEq(Symbol.prototype.valueOf.length, 0);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
