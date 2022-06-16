// |reftest| shell-option(--enable-array-find-last) skip-if(!xulRuntime.shell)
/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var BUGNUMBER = 1704385;
var summary = "Implement %TypedArray%.prototype.{findLast, findLastIndex}";
print(BUGNUMBER + ": " + summary);

const methods = ["findLast", "findLastIndex"];

anyTypedArrayConstructors.forEach(constructor => {
    methods.forEach(method => {
        var arr = new constructor([0, 1, 2, 3, 4, 5]);
        // test that this.length is never called
        Object.defineProperty(arr, "length", {
            get() {
                throw new Error("length accessor called");
            }
        });
        assertEq(arr[method].length, 1);
        assertEq(arr[method](v => v === 3), 3);
        assertEq(arr[method](v => v === 6), method === "findLast" ? undefined : -1);

        var thisValues = [undefined, null, true, 1, "foo", [], {}];
        if (typeof Symbol == "function")
            thisValues.push(Symbol());

        thisValues.forEach(thisArg =>
            assertThrowsInstanceOf(() => arr[method].call(thisArg, () => true), TypeError)
        );

        assertThrowsInstanceOf(() => arr[method](), TypeError);
        assertThrowsInstanceOf(() => arr[method](1), TypeError);
    });
});

anyTypedArrayConstructors.filter(isFloatConstructor).forEach(constructor => {
    var arr = new constructor([-0, 0, 1, 5, NaN, 6]);
    assertEq(arr.findLast(v => Number.isNaN(v)), NaN);
    assertEq(arr.findLastIndex(v => Number.isNaN(v)), 4);

    assertEq(arr.findLast(v => Object.is(v, 0)), 0);
    assertEq(arr.findLastIndex(v => Object.is(v, 0)), 1);

    assertEq(arr.findLast(v => Object.is(v, -0)), -0);
    assertEq(arr.findLastIndex(v => Object.is(v, -0)), 0);
})


if (typeof reportCompare === "function")
    reportCompare(true, true);
