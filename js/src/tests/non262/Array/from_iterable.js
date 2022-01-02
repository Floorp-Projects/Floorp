/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

// Array.from works on arguments objects.
(function () {
    assertDeepEq(Array.from(arguments), ["arg0", "arg1", undefined]);
})("arg0", "arg1", undefined);

// If an object has both .length and [@@iterator] properties, [@@iterator] is used.
var a = ['a', 'e', 'i', 'o', 'u'];
a[Symbol.iterator] = function* () {
    for (var i = 5; i--; )
        yield this[i];
};

var log = '';
function f(x) {
    log += x;
    return x + x;
}

var b = Array.from(a, f);
assertDeepEq(b, ['uu', 'oo', 'ii', 'ee', 'aa']);
assertEq(log, 'uoiea');

// In fact, if [@@iterator] is present, .length isn't queried at all.
var pa = new Proxy(a, {
    has: function (target, id) {
        if (id === "length")
            throw new Error(".length should not be queried (has)");
        return id in target;
    },
    get: function (target, id) {
        if (id === "length")
            throw new Error(".length should not be queried (get)");
        return target[id];
    },
    getOwnPropertyDescriptor: function (target, id) {
        if (id === "length")
            throw new Error(".length should not be queried (getOwnPropertyDescriptor)");
        return Object.getOwnPropertyDescriptor(target, id)
    }
});
log = "";
b = Array.from(pa, f);
assertDeepEq(b, ['uu', 'oo', 'ii', 'ee', 'aa']);
assertEq(log, 'uoiea');

if (typeof reportCompare === 'function')
    reportCompare(0, 0);
