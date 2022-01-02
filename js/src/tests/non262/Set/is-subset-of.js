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

    assertEq(typeof Set.prototype.isSubsetOf, 'function');
    assertEq(Set.prototype.isSubsetOf.length, 1);
    assertEq(Set.prototype.isSubsetOf.name, 'isSubsetOf');

    assertEq(new Set([1, true, null]).isSubsetOf(new Set()), false);

    assertEq(new Set([1, true, null]).isSubsetOf([1, true, null]), true);
    assertEq(new Set([1, 2, 3]).isSubsetOf([2, 3, 4]), false);
    assertEq(new Set([1, 2, 3]).isSubsetOf([1, 2, 3, 4]), true);
    // Works when the argument is a custom iterable which follows the Symbol.iterator protocol
    assertEq(new Set([1, 2, 3]).isSubsetOf(makeArrayIteratorWithHasMethod([3, 4])), false);

    // Works when the `this` is a custom iterable which follows the Symbol.iterator protocol
    assertEq(
        Set.prototype.isSubsetOf.call(
            makeArrayIteratorWithHasMethod([1, 2, 3, 3, 2]),
            makeArrayIteratorWithHasMethod([4, 5, 6])
        ),
        false
    );

    // Does not modify the original set object
    const set = new Set([1]);
    assertEq(set.isSubsetOf(new Set([2])) !== set, true);

    // Argument must be iterable and an object
    assertThrowsInstanceOf(function () {
        const set = new Set();
        set.isSubsetOf();
    }, TypeError);
    for (const arg of [null, {}, true, 1, undefined, NaN, Symbol(), ""]) {
        assertThrowsInstanceOf(function () {
            const set = new Set();
            set.isSubsetOf(arg);
        }, TypeError);
    }

    // `this` must be an Object
    for (const arg of [null, undefined, Symbol()]) {
        assertThrowsInstanceOf(function () {
            Set.prototype.isSubsetOf.call(arg, []);
        }, TypeError);
    }

    // `this` must be iterable
    assertThrowsInstanceOf(function () {
        Set.prototype.isSubsetOf.call({}, []);
    }, TypeError);
} else {
    assertEq(typeof Set.prototype.isSubsetOf, 'undefined');
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
