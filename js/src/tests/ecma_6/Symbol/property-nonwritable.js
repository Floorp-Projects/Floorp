/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

var sym = Symbol.for("moon");
function checkNotWritable(obj) {
    // In sloppy mode, assigning to a nonwritable property silently fails.
    obj[sym] = "portals";
    assertEq(obj[sym], "cheese");

    // In strict mode code, it throws.
    assertThrowsInstanceOf(function () { "use strict"; obj[sym] = "robots"; }, TypeError);
    assertEq(obj[sym], "cheese");
}

var x = {};
Object.defineProperty(x, sym, {
    configurable: true,
    enumerable: true,
    value: "cheese",
    writable: false
});

checkNotWritable(x);

// Assignment can't shadow inherited nonwritable properties either.
var y = Object.create(x);
checkNotWritable(y);
checkNotWritable(Object.create(y));

if (typeof reportCompare === "function")
    reportCompare(0, 0);
