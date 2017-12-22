/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
var expect = undefined;
var actual = (function foo() { "bogus"; })();

reportCompare(expect, actual, "ok");
