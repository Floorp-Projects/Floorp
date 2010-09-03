/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
load(libdir + 'array-compare.js');

var obj = {};

var upper;
function strictAssignAfter(a)
{
  "use strict";
  upper = arguments;
  a = 42;
  return upper;
}

var a1, a2, a3;
for (var i = 0; i < 5; i++)
{
  a1 = strictAssignAfter();
  a2 = strictAssignAfter(17);
  a3 = strictAssignAfter(obj);
}

assertEq(arraysEqual(a1, []), true);
assertEq(arraysEqual(a2, [17]), true);
assertEq(arraysEqual(a3, [obj]), true);
