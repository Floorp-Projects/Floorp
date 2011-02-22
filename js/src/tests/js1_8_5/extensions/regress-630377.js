// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

function f(x) {
    x.q = 1;
}
var a = #1=[f(#1#) for (i in [0])];
assertEq(a.q, 1);
assertEq(a[0], void 0);

reportCompare(0, 0, 'ok');
