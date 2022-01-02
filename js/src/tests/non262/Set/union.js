/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

if (typeof getBuildConfiguration === "undefined") {
    var getBuildConfiguration =
        SpecialPowers.Cu.getJSTestingFunctions().getBuildConfiguration;
}

if (typeof getRealmConfiguration === "undefined") {
    var getRealmConfiguration =
        SpecialPowers.Cu.getJSTestingFunctions().getRealmConfiguration;
}

if (getBuildConfiguration()['new-set-methods'] && getRealmConfiguration().enableNewSetMethods) {

    assertEq(typeof Set.prototype.union, 'function');
    assertEq(Set.prototype.union.length, 1);
    assertEq(Set.prototype.union.name, 'union');

    assertSetContainsExactOrderedItems(new Set([1, true, null]).union(new Set()), [1, true, null]);
    assertSetContainsExactOrderedItems(new Set([1, true, null]).union([1, true, null]), [1, true, null]);
    assertSetContainsExactOrderedItems(new Set([1, 2, 3]).union([2, 3, 4]), [1, 2, 3, 4]);
    assertSetContainsExactOrderedItems(new Set([1, 2, 3]).union([4]), [1, 2, 3, 4]);
    // Works when the argument is a custom iterable which follows the Symbol.iterator protocol
    assertSetContainsExactOrderedItems(new Set([1, 2, 3]).union(makeArrayIterator([3, 4])), [1, 2, 3, 4]);
    assertSetContainsExactOrderedItems(new Set(['a']).union('abc'), ['a', 'b', 'c']);

    // Works when the `this` is a custom iterable which follows the Symbol.iterator protocol
    assertSetContainsExactOrderedItems(Set.prototype.union.call(makeArrayIterator([1, 2, 3, 3, 2]), [4, 5, 6]), [1, 2, 3, 4, 5, 6]);

    // Does not modify the original set object
    const set = new Set([1]);
    assertEq(set.union([2]) !== set, true);

    // Argument must be iterable
    assertThrowsInstanceOf(function () {
        const set = new Set();
        set.union();
    }, TypeError);
    for (const arg of [null, {}, true, 1, undefined, NaN, Symbol()]) {
        assertThrowsInstanceOf(function () {
            const set = new Set();
            set.union(arg);
        }, TypeError);
    }

    // `this` must be an Object
    for (const arg of [null, undefined, Symbol()]) {
        assertThrowsInstanceOf(function () {
            Set.prototype.union.call(arg, []);
        }, TypeError);
    }

    // `this` must be iterable
    assertThrowsInstanceOf(function () {
        Set.prototype.union.call({}, []);
    }, TypeError);
} else {
    assertEq(typeof Set.prototype.union, 'undefined');
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
