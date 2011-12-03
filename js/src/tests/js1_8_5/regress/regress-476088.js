// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/


var err;
try {
    f = function() { var x; x.y; }
    trap(f, 0, "");
    f();
} catch (e) {
    err = e;
}
assertEq(err instanceof TypeError, true);
assertEq(err.message, "x is undefined")

reportCompare(0, 0, 'ok');

