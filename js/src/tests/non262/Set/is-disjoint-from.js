/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

if (typeof getBuildConfiguration === "undefined") {
    var getBuildConfiguration = SpecialPowers.Cu.getJSTestingFunctions().getBuildConfiguration;
}

if (typeof getRealmConfiguration === "undefined") {
    var getRealmConfiguration = SpecialPowers.Cu.getJSTestingFunctions().getRealmConfiguration;
}

if (getBuildConfiguration()['new-set-methods'] && getRealmConfiguration().enableNewSetMethods) {

    assertEq(typeof Set.prototype.isDisjointFrom, 'function');
    assertEq(Set.prototype.isDisjointFrom.length, 1);
    assertEq(Set.prototype.isDisjointFrom.name, 'isDisjointFrom');

    assertEq(new Set([1, true, null]).isDisjointFrom(new Set()), true);
    assertEq(new Set([1, true, null]).isDisjointFrom([1, true, null]), false);
    assertEq(new Set([1, 2, 3]).isDisjointFrom([2, 3, 4]), false);
    assertEq(new Set([1, 2, 3]).isDisjointFrom([4]), true);
    // Works when the argument is a custom iterable which follows the Symbol.iterator protocol
    assertEq(new Set([1, 2, 3]).isDisjointFrom(makeArrayIteratorWithHasMethod([3, 4])), false);
    assertEq(new Set(['a']).isDisjointFrom('abc'), false);

    // Works when the `this` is a custom iterable which follows the Symbol.iterator protocol
    assertEq(Set.prototype.isDisjointFrom.call(makeArrayIteratorWithHasMethod([1, 2, 3, 3, 2]), [4, 5, 6]), true);

    // Does not modify the original set object
    const set = new Set([1]);
    assertEq(set.isDisjointFrom([2]) !== set, true);

    // Argument must be iterable
    assertThrowsInstanceOf(function () {
        const set = new Set();
        set.isDisjointFrom();
    }, TypeError);
    for (const arg of [null, {}, true, 1, undefined, NaN, Symbol()]) {
        assertThrowsInstanceOf(function () {
            const set = new Set();
            set.isDisjointFrom(arg);
        }, TypeError);
    }

    // `this` must be an Object
    for (const arg of [null, undefined, Symbol()]) {
        assertThrowsInstanceOf(function () {
            Set.prototype.isDisjointFrom.call(arg, []);
        }, TypeError);
    }

    // `this` must be iterable
    assertThrowsInstanceOf(function () {
        Set.prototype.isDisjointFrom.call({}, []);
    }, TypeError);
} else {
    assertEq(typeof Set.prototype.isDisjointFrom, 'undefined');
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
