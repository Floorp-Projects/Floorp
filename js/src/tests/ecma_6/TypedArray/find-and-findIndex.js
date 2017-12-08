/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var BUGNUMBER = 1078975;
var summary = "Implement %TypedArray%.prototype.{find, findIndex}";
print(BUGNUMBER + ": " + summary);

const methods = ["find", "findIndex"];

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
        assertEq(arr[method](v => v === 6), method === "find" ? undefined : -1);

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
    assertEq(arr.find(v => Number.isNaN(v)), NaN);
    assertEq(arr.findIndex(v => Number.isNaN(v)), 4);

    assertEq(arr.find(v => Object.is(v, 0)), 0);
    assertEq(arr.findIndex(v => Object.is(v, 0)), 1);

    assertEq(arr.find(v => Object.is(v, -0)), -0);
    assertEq(arr.findIndex(v => Object.is(v, -0)), 0);
})


if (typeof reportCompare === "function")
    reportCompare(true, true);
