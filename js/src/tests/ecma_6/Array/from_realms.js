/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

if (typeof newGlobal === 'function') {
    // G.Array.from, where G is any global, produces an array whose prototype
    // is G.Array.prototype.
    var g = newGlobal();
    var ga = g.Array.from([1, 2, 3]);
    assertEq(ga instanceof g.Array, true);

    // Even if G.Array is not passed in as the 'this' value to the call.
    var from = g.Array.from
    var ga2 = from([1, 2, 3]);
    assertEq(ga2 instanceof g.Array, true);

    // Array.from can be applied to a constructor from another realm.
    var p = Array.from.call(g.Array, [1, 2, 3]);
    assertEq(p instanceof g.Array, true);
    var q = g.Array.from.call(Array, [3, 4, 5]);
    assertEq(q instanceof Array, true);

    // The default 'this' value received by a non-strict mapping function is
    // that function's global, not Array.from's global or the caller's global.
    var h = newGlobal(), result = undefined;
    h.mainGlobal = this;
    h.eval("function f() { mainGlobal.result = this; }");
    g.Array.from.call(Array, [5, 6, 7], h.f);
    // (Give each global in the test a name, for better error messages.)
    this.name = "main";
    g.name = "g";
    h.name = "h";
    assertEq(result.name, "h");
}

if (typeof reportCompare === 'function')
    reportCompare(0, 0);
