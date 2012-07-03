// Giving an Array an own .iterator property affects for-of.

var a = [];
a.iterator = function () {
    yield 'o';
    yield 'k';
};
var s = '';
for (var v of a)
    s += v;
assertEq(s, 'ok');

load(libdir + "asserts.js");
a.iterator = undefined;
assertThrowsInstanceOf(function () { for (var v of a) ; }, TypeError);
