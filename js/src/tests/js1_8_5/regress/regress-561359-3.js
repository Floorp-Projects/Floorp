// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
// Contributor: Jason Orendorff <jorendorff@mozilla.com>

function f(s) {
    with (s)
        return {m: function () { return a; }};
}
var obj = f({a: 'right'});
var a = 'wrong';
assertEq(obj.m(), 'right');

reportCompare(0, 0, 'ok');
