/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
load(libdir + 'array-compare.js');

var obj = {};

function strictAssignOuterParamPSYCH(p)
{
  "use strict";
  function inner(p) { p = 17; }
  inner();
  return arguments;
}

var a1, a2, a3;
for (var i = 0; i < 5; i++)
{
  a1 = strictAssignOuterParamPSYCH();
  a2 = strictAssignOuterParamPSYCH(17);
  a3 = strictAssignOuterParamPSYCH(obj);
}

assertEq(arraysEqual(a1, []), true);
assertEq(arraysEqual(a2, [17]), true);
assertEq(arraysEqual(a3, [obj]), true);
