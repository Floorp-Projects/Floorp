/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var a = [];
for (var i = 0; i < 2; i++) {
    a[i] = {m: function () {}};
    Object.defineProperty(a[i], "m", {configurable: false});
}
assertEq(a[0].m === a[1].m, false);

reportCompare(0, 0, "ok");
