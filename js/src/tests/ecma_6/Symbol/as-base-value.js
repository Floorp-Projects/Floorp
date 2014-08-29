/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

if (typeof Symbol === "function") {
    // Like other primitives, symbols can be treated as objects, using object-like
    // syntax: `symbol.prop` or `symbol[key]`.
    //
    // In ECMAScript spec jargon, this creates a Reference whose base value is a
    // primitive Symbol value.

    var symbols = [
        Symbol(),
        Symbol("ponies"),
        Symbol.for("sym"),
        Symbol.iterator
    ];

    // Test accessor property, used below.
    var gets, sets;
    Object.defineProperty(Symbol.prototype, "prop", {
        get: function () {
            "use strict";
            gets++;
            assertEq(typeof this, "object");
            assertEq(this instanceof Symbol, true);
            assertEq(this.valueOf(), sym);
            return "got";
        },
        set: function (v) {
            "use strict";
            sets++;
            assertEq(typeof this, "object");
            assertEq(this instanceof Symbol, true);
            assertEq(this.valueOf(), sym);
            assertEq(v, "newvalue");
        }
    });

    for (var sym of symbols) {
        assertEq(sym.constructor, Symbol);

        // method on Object.prototype
        assertEq(sym.hasOwnProperty("constructor"), false);
        assertEq(sym.toLocaleString(), sym.toString()); // once .toString() exists

        // custom method monkeypatched onto Symbol.prototype
        Symbol.prototype.nonStrictMethod = function (arg) {
            assertEq(arg, "ok");
            assertEq(this instanceof Symbol, true);
            assertEq(this.valueOf(), sym);
            return 13;
        };
        assertEq(sym.nonStrictMethod("ok"), 13);

        // the same, but strict mode
        Symbol.prototype.strictMethod = function (arg) {
            "use strict";
            assertEq(arg, "ok2");
            assertEq(this, sym);
            return 14;
        };
        assertEq(sym.strictMethod("ok2"), 14);

        // getter/setter on Symbol.prototype
        gets = 0;
        sets = 0;
        var propname = "prop";

        assertEq(sym.prop, "got");
        assertEq(gets, 1);
        assertEq(sym[propname], "got");
        assertEq(gets, 2);

        assertEq(sym.prop = "newvalue", "newvalue");
        assertEq(sets, 1);
        assertEq(sym[propname] = "newvalue", "newvalue");
        assertEq(sets, 2);

        // non-existent property
        assertEq(sym.noSuchProp, undefined);
        var noSuchPropName = "nonesuch";
        assertEq(sym[noSuchPropName], undefined);

        // non-existent method
        assertThrowsInstanceOf(() => sym.noSuchProp(), TypeError);
        assertThrowsInstanceOf(() => sym[noSuchPropName](), TypeError);
    }
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
