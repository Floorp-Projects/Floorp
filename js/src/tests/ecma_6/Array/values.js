/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

if (Array.prototype.values) {
    assertEq(Array.prototype.values, Array.prototype[Symbol.iterator]);
    assertEq(Array.prototype.values.name, "values");
    assertEq(Array.prototype.values.length, 0);

    function valuesUnscopeable() {
        var values = "foo";
        with ([1, 2, 3]) {
            assertEq(indexOf, Array.prototype.indexOf);
            assertEq(values, "foo");
        }
    }
    valuesUnscopeable();
}

if (typeof reportCompare === 'function')
    reportCompare(0, 0);
