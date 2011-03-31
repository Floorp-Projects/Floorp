/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = 'strict-arguments.js';
var BUGNUMBER = 516255;
var summary =
  "ES5 strict mode: arguments objects of strict mode functions must copy " +
  "argument values";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function arrayEvery(arr, fun)
{
  return Array.prototype.every.call(arr, fun);
}

function arraysEqual(a1, a2)
{
  return a1.length === a2.length &&
         arrayEvery(a1, function(v, i) { return v === a2[i]; });
}


/************************
 * NON-STRICT ARGUMENTS *
 ************************/

var obj = {};

function noargs() { return arguments; }

assertEq(arraysEqual(noargs(), []), true);
assertEq(arraysEqual(noargs(1), [1]), true);
assertEq(arraysEqual(noargs(2, obj, 8), [2, obj, 8]), true);

function args(a) { return arguments; }

assertEq(arraysEqual(args(), []), true);
assertEq(arraysEqual(args(1), [1]), true);
assertEq(arraysEqual(args(1, obj), [1, obj]), true);
assertEq(arraysEqual(args("foopy"), ["foopy"]), true);

function assign(a)
{
  a = 17;
  return arguments;
}

assertEq(arraysEqual(assign(1), [17]), true);

function getLaterAssign(a)
{
  var o = arguments;
  a = 17;
  return o;
}

assertEq(arraysEqual(getLaterAssign(1), [17]), true);

function assignElementGetParameter(a)
{
  arguments[0] = 17;
  return a;
}

assertEq(assignElementGetParameter(42), 17);

function assignParameterGetElement(a)
{
  a = 17;
  return arguments[0];
}

assertEq(assignParameterGetElement(42), 17);

function assignArgSub(x, y)
{
  arguments[0] = 3;
  return arguments[0];
}

assertEq(assignArgSub(1), 3);

function assignArgSubParamUse(x, y)
{
  arguments[0] = 3;
  assertEq(x, 3);
  return arguments[0];
}

assertEq(assignArgSubParamUse(1), 3);

function assignArgumentsElement(x, y)
{
  arguments[0] = 3;
  return arguments[Math.random() ? "0" : 0]; // nix arguments[const] optimizations
}

assertEq(assignArgumentsElement(1), 3);

function assignArgumentsElementParamUse(x, y)
{
  arguments[0] = 3;
  assertEq(x, 3);
  return arguments[Math.random() ? "0" : 0]; // nix arguments[const] optimizations
}

assertEq(assignArgumentsElementParamUse(1), 3);

/********************
 * STRICT ARGUMENTS *
 ********************/

function strictNoargs()
{
  "use strict";
  return arguments;
}

assertEq(arraysEqual(strictNoargs(), []), true);
assertEq(arraysEqual(strictNoargs(1), [1]), true);
assertEq(arraysEqual(strictNoargs(1, obj), [1, obj]), true);

function strictArgs(a)
{
  "use strict";
  return arguments;
}

assertEq(arraysEqual(strictArgs(), []), true);
assertEq(arraysEqual(strictArgs(1), [1]), true);
assertEq(arraysEqual(strictArgs(1, obj), [1, obj]), true);

function strictAssign(a)
{
  "use strict";
  a = 17;
  return arguments;
}

assertEq(arraysEqual(strictAssign(), []), true);
assertEq(arraysEqual(strictAssign(1), [1]), true);
assertEq(arraysEqual(strictAssign(1, obj), [1, obj]), true);

var upper;
function strictAssignAfter(a)
{
  "use strict";
  upper = arguments;
  a = 42;
  return upper;
}

assertEq(arraysEqual(strictAssignAfter(), []), true);
assertEq(arraysEqual(strictAssignAfter(17), [17]), true);
assertEq(arraysEqual(strictAssignAfter(obj), [obj]), true);

function strictMaybeAssignOuterParam(p)
{
  "use strict";
  function inner() { p = 17; }
  return arguments;
}

assertEq(arraysEqual(strictMaybeAssignOuterParam(), []), true);
assertEq(arraysEqual(strictMaybeAssignOuterParam(42), [42]), true);
assertEq(arraysEqual(strictMaybeAssignOuterParam(obj), [obj]), true);

function strictAssignOuterParam(p)
{
  "use strict";
  function inner() { p = 17; }
  inner();
  return arguments;
}

assertEq(arraysEqual(strictAssignOuterParam(), []), true);
assertEq(arraysEqual(strictAssignOuterParam(17), [17]), true);
assertEq(arraysEqual(strictAssignOuterParam(obj), [obj]), true);

function strictAssignOuterParamPSYCH(p)
{
  "use strict";
  function inner(p) { p = 17; }
  inner();
  return arguments;
}

assertEq(arraysEqual(strictAssignOuterParamPSYCH(), []), true);
assertEq(arraysEqual(strictAssignOuterParamPSYCH(17), [17]), true);
assertEq(arraysEqual(strictAssignOuterParamPSYCH(obj), [obj]), true);

function strictEval(code, p)
{
  "use strict";
  eval(code);
  return arguments;
}

assertEq(arraysEqual(strictEval("1", 2), ["1", 2]), true);
assertEq(arraysEqual(strictEval("arguments"), ["arguments"]), true);
assertEq(arraysEqual(strictEval("p = 2"), ["p = 2"]), true);
assertEq(arraysEqual(strictEval("p = 2", 17), ["p = 2", 17]), true);
assertEq(arraysEqual(strictEval("arguments[0] = 17"), [17]), true);
assertEq(arraysEqual(strictEval("arguments[0] = 17", 42), [17, 42]), true);

function strictMaybeNestedEval(code, p)
{
  "use strict";
  function inner() { eval(code); }
  return arguments;
}

assertEq(arraysEqual(strictMaybeNestedEval("1", 2), ["1", 2]), true);
assertEq(arraysEqual(strictMaybeNestedEval("arguments"), ["arguments"]), true);
assertEq(arraysEqual(strictMaybeNestedEval("p = 2"), ["p = 2"]), true);
assertEq(arraysEqual(strictMaybeNestedEval("p = 2", 17), ["p = 2", 17]), true);

function strictNestedEval(code, p)
{
  "use strict";
  function inner() { eval(code); }
  inner();
  return arguments;
}

assertEq(arraysEqual(strictNestedEval("1", 2), ["1", 2]), true);
assertEq(arraysEqual(strictNestedEval("arguments"), ["arguments"]), true);
assertEq(arraysEqual(strictNestedEval("p = 2"), ["p = 2"]), true);
assertEq(arraysEqual(strictNestedEval("p = 2", 17), ["p = 2", 17]), true);
assertEq(arraysEqual(strictNestedEval("arguments[0] = 17"), ["arguments[0] = 17"]), true);
assertEq(arraysEqual(strictNestedEval("arguments[0] = 17", 42), ["arguments[0] = 17", 42]), true);

function strictAssignArguments(a)
{
  "use strict";
  arguments[0] = 42;
  return a;
}

assertEq(strictAssignArguments(), undefined);
assertEq(strictAssignArguments(obj), obj);
assertEq(strictAssignArguments(17), 17);

function strictAssignParameterGetElement(a)
{
  "use strict";
  a = 17;
  return arguments[0];
}

assertEq(strictAssignParameterGetElement(42), 42);

function strictAssignArgSub(x, y)
{
  "use strict";
  arguments[0] = 3;
  return arguments[0];
}

assertEq(strictAssignArgSub(1), 3);

function strictAssignArgSubParamUse(x, y)
{
  "use strict";
  arguments[0] = 3;
  assertEq(x, 1);
  return arguments[0];
}

assertEq(strictAssignArgSubParamUse(1), 3);

function strictAssignArgumentsElement(x, y)
{
  "use strict";
  arguments[0] = 3;
  return arguments[Math.random() ? "0" : 0]; // nix arguments[const] optimizations
}

assertEq(strictAssignArgumentsElement(1), 3);

function strictAssignArgumentsElementParamUse(x, y)
{
  "use strict";
  arguments[0] = 3;
  assertEq(x, 1);
  return arguments[Math.random() ? "0" : 0]; // nix arguments[const] optimizations
}

assertEq(strictAssignArgumentsElementParamUse(1), 3);

function strictNestedAssignShadowVar(p)
{
  "use strict";
  function inner()
  {
    var p = 12;
    function innermost() { p = 1776; return 12; }
    return innermost();
  }
  return arguments;
}

assertEq(arraysEqual(strictNestedAssignShadowVar(), []), true);
assertEq(arraysEqual(strictNestedAssignShadowVar(99), [99]), true);
assertEq(arraysEqual(strictNestedAssignShadowVar(""), [""]), true);
assertEq(arraysEqual(strictNestedAssignShadowVar(obj), [obj]), true);

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

assertEq(arraysEqual(strictNestedAssignShadowCatch(), []), true);
assertEq(arraysEqual(strictNestedAssignShadowCatch(99), [99]), true);
assertEq(arraysEqual(strictNestedAssignShadowCatch(""), [""]), true);
assertEq(arraysEqual(strictNestedAssignShadowCatch(obj), [obj]), true);

function strictNestedAssignShadowCatchCall(p)
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
  inner();
  return arguments;
}

assertEq(arraysEqual(strictNestedAssignShadowCatchCall(), []), true);
assertEq(arraysEqual(strictNestedAssignShadowCatchCall(99), [99]), true);
assertEq(arraysEqual(strictNestedAssignShadowCatchCall(""), [""]), true);
assertEq(arraysEqual(strictNestedAssignShadowCatchCall(obj), [obj]), true);

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

assertEq(arraysEqual(strictNestedAssignShadowFunction(), []), true);
assertEq(arraysEqual(strictNestedAssignShadowFunction(99), [99]), true);
assertEq(arraysEqual(strictNestedAssignShadowFunction(""), [""]), true);
assertEq(arraysEqual(strictNestedAssignShadowFunction(obj), [obj]), true);

function strictNestedAssignShadowFunctionCall(p)
{
  "use strict";
  function inner()
  {
    function p() { }
    p = 1776;
  }
  return arguments;
}

assertEq(arraysEqual(strictNestedAssignShadowFunctionCall(), []), true);
assertEq(arraysEqual(strictNestedAssignShadowFunctionCall(99), [99]), true);
assertEq(arraysEqual(strictNestedAssignShadowFunctionCall(""), [""]), true);
assertEq(arraysEqual(strictNestedAssignShadowFunctionCall(obj), [obj]), true);

function strictNestedShadowAndMaybeEval(code, p)
{
  "use strict";
  function inner(p) { eval(code); }
  return arguments;
}

assertEq(arraysEqual(strictNestedShadowAndMaybeEval("1", 2), ["1", 2]), true);
assertEq(arraysEqual(strictNestedShadowAndMaybeEval("arguments"), ["arguments"]), true);
assertEq(arraysEqual(strictNestedShadowAndMaybeEval("p = 2"), ["p = 2"]), true);
assertEq(arraysEqual(strictNestedShadowAndMaybeEval("p = 2", 17), ["p = 2", 17]), true);
assertEq(arraysEqual(strictNestedShadowAndMaybeEval("arguments[0] = 17"), ["arguments[0] = 17"]), true);
assertEq(arraysEqual(strictNestedShadowAndMaybeEval("arguments[0] = 17", 42), ["arguments[0] = 17", 42]), true);

function strictNestedShadowAndEval(code, p)
{
  "use strict";
  function inner(p) { eval(code); }
  return arguments;
}

assertEq(arraysEqual(strictNestedShadowAndEval("1", 2), ["1", 2]), true);
assertEq(arraysEqual(strictNestedShadowAndEval("arguments"), ["arguments"]), true);
assertEq(arraysEqual(strictNestedShadowAndEval("p = 2"), ["p = 2"]), true);
assertEq(arraysEqual(strictNestedShadowAndEval("p = 2", 17), ["p = 2", 17]), true);
assertEq(arraysEqual(strictNestedShadowAndEval("arguments[0] = 17"), ["arguments[0] = 17"]), true);
assertEq(arraysEqual(strictNestedShadowAndEval("arguments[0] = 17", 42), ["arguments[0] = 17", 42]), true);

function strictEvalContainsMutation(code)
{
  "use strict";
  return eval(code);
}

assertEq(arraysEqual(strictEvalContainsMutation("code = 17; arguments"), ["code = 17; arguments"]), true);
assertEq(arraysEqual(strictEvalContainsMutation("arguments[0] = 17; arguments"), [17]), true);
assertEq(strictEvalContainsMutation("arguments[0] = 17; code"), "arguments[0] = 17; code");

function strictNestedAssignShadowFunctionName(p)
{
  "use strict";
  function inner()
  {
    function p() { p = 1776; }
    p();
  }
  inner();
  return arguments;
}

assertEq(arraysEqual(strictNestedAssignShadowFunctionName(), []), true);
assertEq(arraysEqual(strictNestedAssignShadowFunctionName(99), [99]), true);
assertEq(arraysEqual(strictNestedAssignShadowFunctionName(""), [""]), true);
assertEq(arraysEqual(strictNestedAssignShadowFunctionName(obj), [obj]), true);


/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
