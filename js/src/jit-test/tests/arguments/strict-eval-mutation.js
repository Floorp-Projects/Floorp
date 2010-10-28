/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
load(libdir + 'array-compare.js');

var obj = {};

function strictEvalMutation(code)
{
  "use strict";
  return eval(code);
}

var a1, a2;
for (var i = 0; i < 5; i++)
{
  a1 = strictEvalMutation("code = 17; arguments");
  a2 = strictEvalMutation("arguments[0] = 17; arguments");
  assertEq(strictEvalMutation("arguments[0] = 17; code"), "arguments[0] = 17; code");
}

assertEq(arraysEqual(a1, ["code = 17; arguments"]), true);
assertEq(arraysEqual(a2, [17]), true);
