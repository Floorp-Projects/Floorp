/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

if (typeof Symbol === "function") {
    // Well-known symbols exist.
    assertEq(typeof Symbol.iterator, "symbol");

    // They are never in the registry.
    assertEq(Symbol.iterator !== Symbol.for("Symbol.iterator"), true);

    // They are shared across realms.
    if (typeof Realm === 'function')
        throw new Error("please update this test to use Realms");
    if (typeof newGlobal === 'function') {
        var g = newGlobal();
        assertEq(Symbol.iterator, g.Symbol.iterator);
    }
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
