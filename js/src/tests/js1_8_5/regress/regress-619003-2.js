/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
var expect = "global";
var actual = expect;
function f([actual]) { }
f(["local"]);
reportCompare(expect, actual, "ok");
