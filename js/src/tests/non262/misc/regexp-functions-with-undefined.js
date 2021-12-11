/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var a = /undefined/.exec();
assertEq(a[0], 'undefined');
assertEq(a.length, 1);

a = /undefined/.exec(undefined);
assertEq(a[0], 'undefined');
assertEq(a.length, 1);

assertEq(/undefined/.test(), true);
assertEq(/undefined/.test(undefined), true);

assertEq(/aaaa/.exec(), null);
assertEq(/aaaa/.exec(undefined), null);

assertEq(/aaaa/.test(), false);
assertEq(/aaaa/.test(undefined), false);


assertEq("undefined".search(), 0);
assertEq("undefined".search(undefined), 0);
assertEq("aaaa".search(), 0);
assertEq("aaaa".search(undefined), 0);

a = "undefined".match();
assertEq(a[0], "");
assertEq(a.length, 1);
a = "undefined".match(undefined);
assertEq(a[0], "");
assertEq(a.length, 1);
a = "aaaa".match();
assertEq(a[0], "");
assertEq(a.length, 1);
a = "aaaa".match(undefined);
assertEq(a[0], "");
assertEq(a.length, 1);

if (typeof reportCompare === "function")
    reportCompare(true, true);
