/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

// Symbols can be shared across realms.

if (typeof Reflect !== "undefined" && typeof Reflect.Realm === "function") {
    throw new Error("Congratulations on implementing Reflect.Realm! " +
                    "Please update this test to use it.");
}
if (typeof newGlobal === "function") {
    var g = newGlobal();
    var gj = g.eval("jones = Symbol('jones')");
    assertEq(typeof gj, "symbol");
    assertEq(g.jones, g.jones);
    assertEq(gj, g.jones);
    assertEq(gj !== Symbol("jones"), true);

    // A symbol can be round-tripped to another realm and back;
    // the result is the original symbol.
    var smith = Symbol("smith");
    g.smith = smith;  // put smith into the realm
    assertEq(g.smith, smith);  // pull it back out

    // Spot-check that non-generic methods can be applied to symbols and Symbol
    // objects from other realms.
    assertEq(Symbol.prototype.toString.call(gj), "Symbol(jones)");
    assertEq(Symbol.prototype.toString.call(g.eval("Object(Symbol('brown'))")),
             "Symbol(brown)");

    // Symbol.for functions share a symbol registry across all realms.
    assertEq(g.Symbol.for("ponies"), Symbol.for("ponies"));
    assertEq(g.eval("Symbol.for('rainbows')"), Symbol.for("rainbows"));
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
