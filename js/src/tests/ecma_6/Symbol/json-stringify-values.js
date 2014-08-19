/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

if (typeof Symbol === "function") {
    // To JSON.stringify, symbols are the same as undefined.

    var symbols = [
        Symbol(),
        Symbol.for("ponies"),
        Symbol.iterator
    ];

    for (var sym of symbols) {
        assertEq(JSON.stringify(sym), undefined);
        assertEq(JSON.stringify([sym]), "[null]");

        // JSON.stringify skips symbol-valued properties!
        assertEq(JSON.stringify({x: sym}), '{}');

        // However such properties are passed to the replacerFunction if any.
        var replacer = function (key, val) {
            assertEq(typeof this, "object");
            if (typeof val === "symbol") {
                assertEq(val, sym);
                return "ding";
            }
            return val;
        };
        assertEq(JSON.stringify(sym, replacer), '"ding"');
        assertEq(JSON.stringify({x: sym}, replacer), '{"x":"ding"}');
    }
}

if (typeof reportCompare === 'function')
    reportCompare(0, 0, 'ok');
