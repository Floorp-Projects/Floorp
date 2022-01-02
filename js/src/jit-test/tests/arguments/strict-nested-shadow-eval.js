/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
load(libdir + 'array-compare.js');

function strictNestedShadowEval(code, p)
{
  "use strict";
  function inner(p) { eval(code); }
  return arguments;
}

var a1, a2, a3, a4, a5, a6;
for (var i = 0; i < 5; i++)
{
  a1 = strictNestedShadowEval("1", 2);
  a2 = strictNestedShadowEval("arguments");
  a3 = strictNestedShadowEval("p = 2");
  a4 = strictNestedShadowEval("p = 2", 17);
  a5 = strictNestedShadowEval("arguments[0] = 17");
  a6 = strictNestedShadowEval("arguments[0] = 17", 42);
}

assertEq(arraysEqual(a1, ["1", 2]), true);
assertEq(arraysEqual(a2, ["arguments"]), true);
assertEq(arraysEqual(a3, ["p = 2"]), true);
assertEq(arraysEqual(a4, ["p = 2", 17]), true);
assertEq(arraysEqual(a5, ["arguments[0] = 17"]), true);
assertEq(arraysEqual(a6, ["arguments[0] = 17", 42]), true);
