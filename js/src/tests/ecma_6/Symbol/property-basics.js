/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

if (typeof Symbol === "function") {
    var symbols = [
        Symbol(),
        Symbol("one"),
        Symbol.for("two"),
        Symbol.iterator
    ];

    for (var sym of symbols) {
        var obj = {};

        // access a nonexistent property
        assertEq(sym in obj, false);
        assertEq(obj.hasOwnProperty(sym), false);
        assertEq(obj[sym], undefined);
        assertEq(typeof obj[sym], "undefined");
        assertEq(Object.getOwnPropertyDescriptor(obj, sym), undefined);

        // assign, then try accessing again
        obj[sym] = "ok";
        assertEq(sym in obj, true);
        assertEq(obj.hasOwnProperty(sym), true);
        assertEq(obj[sym], "ok");
        assertDeepEq(Object.getOwnPropertyDescriptor(obj, sym), {
            configurable: true,
            enumerable: true,
            value: "ok",
            writable: true
        });

        // assign again, observe value is overwritten
        obj[sym] = 12;
        assertEq(obj[sym], 12);

        // increment
        assertEq(obj[sym]++, 12);
        assertEq(obj[sym], 13);
    }
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
