// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

assertThrowsInstanceOf(function() { eval('let a = 100_00_;'); }, SyntaxError);
assertThrowsInstanceOf(() => eval("let b = 10__;"), SyntaxError);
assertThrowsInstanceOf(() => eval("let b = 1._2;"), SyntaxError);

if (typeof reportCompare === "function")
  reportCompare(true, true);

