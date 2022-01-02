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

    assertEq(typeof Set.prototype.intersection, 'function');
    assertEq(Set.prototype.intersection.length, 1);
    assertEq(Set.prototype.intersection.name, 'intersection');

    assertSetContainsExactOrderedItems(new Set([1, true, null]).intersection(new Set()), []);
    assertSetContainsExactOrderedItems(new Set([1, true, null]).intersection([1, true, null]), [1, true, null]);
    assertSetContainsExactOrderedItems(new Set([1, 2, 3]).intersection([2, 3, 4]), [2, 3]);
    assertSetContainsExactOrderedItems(new Set([1, 2, 3]).intersection([4]), []);
    // Works when the argument is a custom iterable which follows the Symbol.iterator protocol
    assertSetContainsExactOrderedItems(new Set([1, 2, 3]).intersection(makeArrayIteratorWithHasMethod([3, 4])), [3]);
    assertSetContainsExactOrderedItems(new Set(['a']).intersection('abc'), ['a']);

    // Works when the `this` is a custom iterable which follows the Symbol.iterator protocol and has a `has` method
    assertSetContainsExactOrderedItems(Set.prototype.intersection.call(makeArrayIteratorWithHasMethod([1, 2, 3, 3, 2]), [3, 4, 5, 6]), [3]);

    // Does not modify the original set object
    const set = new Set([1]);
    assertEq(set.intersection([2]) !== set, true);

    // Argument must be iterable
    assertThrowsInstanceOf(function () {
        const set = new Set();
        set.intersection();
    }, TypeError);
    for (const arg of [null, {}, true, 1, undefined, NaN, Symbol()]) {
        assertThrowsInstanceOf(function () {
            const set = new Set();
            set.intersection(arg);
        }, TypeError);
    }

    // `this` must be an Object
    for (const arg of [null, undefined, Symbol()]) {
        assertThrowsInstanceOf(function () {
            Set.prototype.intersection.call(arg, []);
        }, TypeError);
    }

    // `this` must be iterable
    assertThrowsInstanceOf(function () {
        Set.prototype.intersection.call({}, []);
    }, TypeError);
} else {
    assertEq(typeof Set.prototype.intersection, 'undefined');
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
