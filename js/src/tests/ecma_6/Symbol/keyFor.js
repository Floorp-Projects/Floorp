/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

if (typeof Symbol === "function") {
    assertEq(Symbol.keyFor(Symbol.for("moon")), "moon");
    assertEq(Symbol.keyFor(Symbol.for("")), "");
    assertEq(Symbol.keyFor(Symbol("moon")), undefined);
    assertEq(Symbol.keyFor(Symbol.iterator), undefined);

    assertThrowsInstanceOf(() => Symbol.keyFor(), TypeError);
    assertThrowsInstanceOf(() => Symbol.keyFor(Object(Symbol("moon"))), TypeError);

    assertEq(Symbol.keyFor.length, 1);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
