/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
load(libdir + 'array-compare.js');

var obj = {};

function strictAssignArgumentsElement(a)
{
  "use strict";
  arguments[0] = 42;
  return a;
}

for (var i = 0; i < 5; i++)
{
  assertEq(strictAssignArgumentsElement(), undefined);
  assertEq(strictAssignArgumentsElement(obj), obj);
  assertEq(strictAssignArgumentsElement(17), 17);
}
