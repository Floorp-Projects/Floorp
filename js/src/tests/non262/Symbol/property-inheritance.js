/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

var sym = Symbol.for("hello");
function F() {}
var f = new F();

// inherited data property
F.prototype[sym] = "world";
assertEq(sym in f, true);
assertEq(f.hasOwnProperty(sym), false);
assertEq(f[sym], "world");

// shadowing assignment
f[sym] = "kitty";
assertEq(f[sym], "kitty");
assertEq(F.prototype[sym], "world");

// deletion, revealing previously shadowed property
assertEq(delete f[sym], true);
assertEq(f.hasOwnProperty(sym), false);
assertEq(f[sym], "world");

// inherited accessor property
var value = undefined;
Object.defineProperty(F.prototype, sym, {
    configurable: true,
    get: function () { return 23; },
    set: function (v) { value = v; }
});
assertEq(sym in f, true);
assertEq(f.hasOwnProperty(sym), false);
assertEq(f[sym], 23);
f[sym] = "gravity";
assertEq(value, "gravity");

// inherited accessor property with no setter
Object.defineProperty(F.prototype, sym, {
    set: undefined
});
assertThrowsInstanceOf(function () { "use strict"; f[sym] = 0; }, TypeError);

// deeply inherited accessor property
var g = Object.create(f);
for (var i = 0; i < 100; i++)
    g = Object.create(g);
assertEq(g[sym], 23);

if (typeof reportCompare === "function")
    reportCompare(0, 0);
