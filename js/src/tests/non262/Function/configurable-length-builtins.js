/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

// Deleting .length from a variety of builtin functions works as expected.
for (var fun of [Math.sin, Array.prototype.map, eval]) {
    assertEq(delete fun.length, true);
    assertEq(fun.hasOwnProperty("length"), false);
    assertEq("length" in fun, true);  // still inheriting Function.prototype.length
    assertEq(fun.length, 0);

    // The inherited property is nonwritable, so assigning still fails
    // (silently, in sloppy mode).
    fun.length = Math.hypot;
    assertEq(fun.length, 0);

    // It can be shadowed via defineProperty.
    Object.defineProperty(fun, "length", {value: Math.hypot});
    assertEq(fun.length, Math.hypot);
}

reportCompare(0, 0, 'ok');
