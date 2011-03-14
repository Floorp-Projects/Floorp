// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
// Contributor: Jason Orendorff <jorendorff@mozilla.com>

function f(s) {
    var obj = {m: function () { return a; }};
    eval(s);
    return obj;
}
var obj = f("var a = 'right';");
var a = 'wrong';
assertEq(obj.m(), 'right');

reportCompare(0, 0, 'ok');
