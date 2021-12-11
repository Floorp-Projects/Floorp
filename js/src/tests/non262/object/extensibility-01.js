/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Jeff Walden <jwalden+code@mit.edu>
 */

var gTestfile = '15.2.3.10-01.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 492849;
var summary = 'ES5: Implement Object.preventExtensions, Object.isExtensible';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function trySetProperty(o, p, v, strict)
{
  function strictSetProperty()
  {
    "use strict";
    o[p] = v;
  }

  function setProperty()
  {
    o[p] = v;
  }

  assertEq(Object.prototype.hasOwnProperty.call(o, p), false);

  try
  {
    if (strict)
      strictSetProperty();
    else
      setProperty();
    if (o[p] === v)
      return "set";
    if (p in o)
      return "set-converted";
    return "swallowed";
  }
  catch (e)
  {
    return "throw";
  }
}

function tryDefineProperty(o, p, v)
{
  assertEq(Object.prototype.hasOwnProperty.call(o, p), false);

  try
  {
    Object.defineProperty(o, p, { value: v });
    if (o[p] === v)
      return "set";
    if (p in o)
      return "set-converted";
    return "swallowed";
  }
  catch (e)
  {
    return "throw";
  }
}

assertEq(typeof Object.preventExtensions, "function");
assertEq(Object.preventExtensions.length, 1);

var slowArray = [1, 2, 3];
slowArray.slow = 5;
var objs =
  [{}, { 1: 2 }, { a: 3 }, [], [1], [, 1], slowArray, function a(){}, /a/];

for (var i = 0, sz = objs.length; i < sz; i++)
{
  var o = objs[i];
  assertEq(Object.isExtensible(o), true, "object " + i + " not extensible?");

  var o2 = Object.preventExtensions(o);
  assertEq(o, o2);

  assertEq(Object.isExtensible(o), false, "object " + i + " is extensible?");

  assertEq(trySetProperty(o, "baz", 17, true), "throw",
           "unexpected behavior for strict-mode property-addition to " +
           "object " + i);
  assertEq(trySetProperty(o, "baz", 17, false), "swallowed",
           "unexpected behavior for property-addition to object " + i);

  assertEq(tryDefineProperty(o, "baz", 17), "throw",
           "unexpected behavior for new property definition on object " + i);
}

/******************************************************************************/

reportCompare(true, true);

print("All tests passed!");
