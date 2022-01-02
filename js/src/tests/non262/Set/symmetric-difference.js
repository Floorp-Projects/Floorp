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

    assertEq(typeof Set.prototype.symmetricDifference, 'function');
    assertEq(Set.prototype.symmetricDifference.length, 1);
    assertEq(Set.prototype.symmetricDifference.name, 'symmetricDifference');

    assertSetContainsExactOrderedItems(new Set([1, true, null]).symmetricDifference(new Set()), [1, true, null]);
    assertSetContainsExactOrderedItems(new Set([1, true, null]).symmetricDifference([1, true, null]), []);
    assertSetContainsExactOrderedItems(new Set([1, 2, 3]).symmetricDifference([2, 3, 4]), [1, 4]);
    assertSetContainsExactOrderedItems(new Set([1, 2, 3]).symmetricDifference([4]), [1, 2, 3, 4]);
    // Works when the argument is a custom iterable which follows the Symbol.iterator protocol
    assertSetContainsExactOrderedItems(new Set([1, 2, 3]).symmetricDifference(makeArrayIterator([3, 4])), [1, 2, 4]);
    assertSetContainsExactOrderedItems(new Set(['a']).symmetricDifference('abc'), ['b', 'c']);

    // Works when the `this` is a custom iterable which follows the Symbol.iterator protocol
    assertSetContainsExactOrderedItems(Set.prototype.symmetricDifference.call(makeArrayIterator([1, 2, 3, 3, 2]), [4, 5, 6]), [1, 2, 3, 4, 5, 6]);

    // Does not modify the original set object
    const set = new Set([1]);
    assertEq(set.symmetricDifference([2]) !== set, true);

    // Argument must be iterable
    assertThrowsInstanceOf(function () {
        const set = new Set();
        set.symmetricDifference();
    }, TypeError);
    for (const arg of [null, {}, true, 1, undefined, NaN, Symbol()]) {
        assertThrowsInstanceOf(function () {
            const set = new Set();
            set.symmetricDifference(arg);
        }, TypeError);
    }

    // `this` must be an Object
    for (const arg of [null, undefined, Symbol()]) {
        assertThrowsInstanceOf(function () {
            Set.prototype.symmetricDifference.call(arg, []);
        }, TypeError);
    }

    // `this` must be iterable
    assertThrowsInstanceOf(function () {
        Set.prototype.symmetricDifference.call({}, []);
    }, TypeError);
} else {
    assertEq(typeof Set.prototype.symmetricDifference, 'undefined');
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
