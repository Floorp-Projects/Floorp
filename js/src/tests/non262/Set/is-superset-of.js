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

    assertEq(typeof Set.prototype.isSupersetOf, 'function');
    assertEq(Set.prototype.isSupersetOf.length, 1);
    assertEq(Set.prototype.isSupersetOf.name, 'isSupersetOf');

    assertEq(new Set([1, true, null]).isSupersetOf(new Set()), true);
    assertEq(new Set([1, true, null]).isSupersetOf([1, true, null]), true);
    assertEq(new Set([1, 2, 3]).isSupersetOf([2, 3, 4]), false);
    assertEq(new Set([1, 2, 3]).isSupersetOf([3]), true);
    // Works when the argument is a custom iterable which follows the Symbol.iterator protocol
    assertEq(new Set([1, 2, 3]).isSupersetOf(makeArrayIteratorWithHasMethod([2, 3])), true);
    assertEq(new Set(['a']).isSupersetOf('abc'), false);

    // Works when the `this` is a custom iterable which follows the Symbol.iterator protocol
    assertEq(Set.prototype.isSupersetOf.call(makeArrayIteratorWithHasMethod([1, 2, 3, 3, 2]), [4, 5, 6]), false);

    // Does not modify the original set object
    const set = new Set([1]);
    assertEq(set.isSupersetOf([2]) !== set, true);

    // Argument must be iterable
    assertThrowsInstanceOf(function () {
        const set = new Set();
        set.isSupersetOf();
    }, TypeError);
    for (const arg of [null, {}, true, 1, undefined, NaN, Symbol()]) {
        assertThrowsInstanceOf(function () {
            const set = new Set();
            set.isSupersetOf(arg);
        }, TypeError);
    }

    // `this` must be an Object
    for (const arg of [null, undefined, Symbol()]) {
        assertThrowsInstanceOf(function () {
            Set.prototype.isSupersetOf.call(arg, []);
        }, TypeError);
    }

    // `this` must be iterable
    assertThrowsInstanceOf(function () {
        Set.prototype.isSupersetOf.call({}, []);
    }, TypeError);
} else {
    assertEq(typeof Set.prototype.isSupersetOf, 'undefined');
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
