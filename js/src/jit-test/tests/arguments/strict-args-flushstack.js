/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
var args;

function test()
{
  "use strict";
  eval("args = arguments;");
  var a = [];
  for (var i = 0; i < 9; i++)
    a.push(arguments);
  return a;
}

var a = test();

assertEq(Array.isArray(a), true);
assertEq(a.length, 9);

var count = 0;
a.forEach(function(v, i) { count++; assertEq(v, args); });
assertEq(count, 9);

assertEq(Object.prototype.toString.call(args), "[object Arguments]");
assertEq(args.length, 0);
