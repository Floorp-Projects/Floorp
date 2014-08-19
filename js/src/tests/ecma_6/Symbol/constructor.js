/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

if (typeof Symbol === "function") {
    // Symbol(symbol) throws a TypeError.
    var sym = Symbol();
    assertThrowsInstanceOf(() => Symbol(sym), TypeError);

    // Symbol(undefined) is equivalent to Symbol().
    assertEq(Symbol(undefined).toString(), "Symbol()");

    // Otherwise, Symbol(v) means Symbol(ToString(v)).
    assertEq(Symbol(7).toString(), "Symbol(7)");
    assertEq(Symbol(true).toString(), "Symbol(true)");
    assertEq(Symbol(null).toString(), "Symbol(null)");
    assertEq(Symbol([1, 2]).toString(), "Symbol(1,2)");
    var symobj = Object(sym);
    assertThrowsInstanceOf(() => Symbol(symobj), TypeError);

    var hits = 0;
    var obj = {
        toString: function () {
            hits++;
            return "ponies";
        }
    };
    assertEq(Symbol(obj).toString(), "Symbol(ponies)");
    assertEq(hits, 1);

    assertEq(Object.getPrototypeOf(Symbol.prototype), Object.prototype);

    // Symbol.prototype is not itself a Symbol object.
    assertThrowsInstanceOf(() => Symbol.prototype.valueOf(), TypeError);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
