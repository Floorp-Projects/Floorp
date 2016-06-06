/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

var names = [
    "isConcatSpreadable",
    "iterator",
    "match",
    "replace",
    "search",
    "species",
    "hasInstance",
    "split",
    "toPrimitive",
    "unscopables"
];

for (var name of names) {
    // Well-known symbols exist.
    assertEq(typeof Symbol[name], "symbol");

    // They are never in the registry.
    assertEq(Symbol[name] !== Symbol.for("Symbol." + name), true);

    // They are shared across realms.
    if (typeof Realm === 'function')
        throw new Error("please update this test to use Realms");
    if (typeof newGlobal === 'function') {
        var g = newGlobal();
        assertEq(Symbol[name], g.Symbol[name]);
    }

    // Descriptor is all false.
    var desc = Object.getOwnPropertyDescriptor(Symbol, name);
    assertEq(typeof desc.value, "symbol");
    assertEq(desc.writable, false);
    assertEq(desc.enumerable, false);
    assertEq(desc.configurable, false);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
