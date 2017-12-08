/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jason Orendorff
 */

function f(x) { return 1 + "" + (x + 1); }
reportCompare("12", f(1), "");
var g = eval("(" + f + ")");
reportCompare("12", g(1), "");
