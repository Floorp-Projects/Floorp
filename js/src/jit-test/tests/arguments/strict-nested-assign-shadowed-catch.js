/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
load(libdir + 'array-compare.js');

var obj = {};

function strictNestedAssignShadowCatch(p)
{
  "use strict";
  function inner()
  {
    try
    {
    }
    catch (p)
    {
      var f = function innermost() { p = 1776; return 12; };
      f();
    }
  }
  return arguments;
}

var a1, a2, a3, a4;
for (var i = 0; i < 5; i++)
{
  a1 = strictNestedAssignShadowCatch();
  a2 = strictNestedAssignShadowCatch(99);
  a3 = strictNestedAssignShadowCatch("");
  a4 = strictNestedAssignShadowCatch(obj);
}

assertEq(arraysEqual(a1, []), true);
assertEq(arraysEqual(a2, [99]), true);
assertEq(arraysEqual(a3, [""]), true);
assertEq(arraysEqual(a4, [obj]), true);

