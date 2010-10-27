/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
load(libdir + 'array-compare.js');

var obj = {};

function strictNestedAssignShadowFunction(p)
{
  "use strict";
  function inner()
  {
    function p() { }
    p = 1776;
  }
  return arguments;
}

var a1, a2, a3, a4;
for (var i = 0; i < 5; i++)
{
  a1 = strictNestedAssignShadowFunction();
  a2 = strictNestedAssignShadowFunction(99);
  a3 = strictNestedAssignShadowFunction("");
  a4 = strictNestedAssignShadowFunction(obj);
}

assertEq(arraysEqual(a1, []), true);
assertEq(arraysEqual(a2, [99]), true);
assertEq(arraysEqual(a3, [""]), true);
assertEq(arraysEqual(a4, [obj]), true);
