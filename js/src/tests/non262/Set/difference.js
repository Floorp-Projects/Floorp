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

    assertEq(typeof Set.prototype.difference, 'function');
    assertEq(Set.prototype.difference.length, 1);
    assertEq(Set.prototype.difference.name, 'difference');

    assertSetContainsExactOrderedItems(new Set([1, true, null]).difference(new Set()), [1, true, null]);
    assertSetContainsExactOrderedItems(new Set([1, true, null]).difference([1, true, null]), []);
    assertSetContainsExactOrderedItems(new Set([1, 2, 3]).difference([2, 3, 4]), [1]);
    assertSetContainsExactOrderedItems(new Set([1, 2, 3]).difference([4]), [1, 2, 3]);
    // Works when the argument is a custom iterable which follows the Symbol.iterator protocol
    assertSetContainsExactOrderedItems(new Set([1, 2, 3]).difference(makeArrayIterator([3, 4])), [1, 2]);
    assertSetContainsExactOrderedItems(new Set(['a','d']).difference('abc'), ['d']);

    // Works when the `this` is a custom iterable which follows the Symbol.iterator protocol
    assertSetContainsExactOrderedItems(Set.prototype.difference.call(makeArrayIterator([1, 2, 3, 3, 2]), [4, 5, 6]), [1, 2, 3]);

    // Does not modify the original set object
    const set = new Set([1]);
    assertEq(set.difference([1]) !== set, true);

    // Argument must be iterable
    assertThrowsInstanceOf(function () {
        const set = new Set();
        set.difference();
    }, TypeError);
    for (const arg of [null, {}, true, 1, undefined, NaN, Symbol()]) {
        assertThrowsInstanceOf(function () {
            const set = new Set();
            set.difference(arg);
        }, TypeError);
    }

    // `this` must be an Object
    for (const arg of [null, undefined, Symbol()]) {
        assertThrowsInstanceOf(function () {
            Set.prototype.difference.call(arg, []);
        }, TypeError);
    }

    // `this` must be iterable
    assertThrowsInstanceOf(function () {
        Set.prototype.difference.call({}, []);
    }, TypeError);
} else {
    assertEq(typeof Set.prototype.difference, 'undefined');
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
