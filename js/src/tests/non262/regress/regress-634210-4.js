/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function outer() {
    function f() {}
    f.p = function() {};
    Object.seal(f);
    return f.p;
}

var m1 = outer();
var m2 = outer();
assertEq(m1 === m2, false);

m1.foo = "hi";
assertEq(m2.foo, undefined);

reportCompare(0, 0, "ok");
