// Giving an Array an own .iterator property affects for-of.

load(libdir + "asserts.js");
load(libdir + "iteration.js");

var a = [];
a[Symbol.iterator] = function* () {
    yield 'o';
    yield 'k';
};
var s = '';
for (var v of a)
    s += v;
assertEq(s, 'ok');

a[Symbol.iterator] = undefined;
assertThrowsInstanceOf(function () { for (var v of a) ; }, TypeError);
