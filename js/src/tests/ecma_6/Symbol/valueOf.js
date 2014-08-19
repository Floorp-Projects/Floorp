/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

if (typeof Symbol === "function") {
    var symbols = [
        Symbol(),
        Symbol("ok"),
        Symbol.for("dummies"),
        Symbol.iterator
    ];

    for (var sym of symbols) {
        assertEq(sym.valueOf(), sym);
        assertEq(Object(sym).valueOf(), sym);
    }

    // Any other value throws.
    var nonsymbols = [undefined, null, NaN, {}, Symbol.prototype];
    for (var nonsym of nonsymbols)
        assertThrowsInstanceOf(() => Symbol.prototype.valueOf.call(nonsym), TypeError);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
