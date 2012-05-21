/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 505587;
var summary = 'ES5 Object.getOwnPropertyDescriptor(O)';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

/**************
 * BEGIN TEST *
 **************/

function assertEq(a, e, msg)
{
  function SameValue(v1, v2)
  {
    if (v1 === 0 && v2 === 0)
      return 1 / v1 === 1 / v2;
    if (v1 !== v1 && v2 !== v2)
      return true;
    return v1 === v2;
  }

  if (!SameValue(a, e))
  {
    var stack = new Error().stack || "";
    throw "Assertion failed: got " + a + ", expected " + e +
          (msg ? ": " + msg : "") +
          (stack ? "\nStack:\n" + stack : "");
  }
}

function expectDescriptor(actual, expected)
{
  if (actual === undefined && expected === undefined)
    return;

  assertEq(typeof actual, "object");
  assertEq(typeof expected, "object");

  var fields =
    {
      value: true,
      get: true,
      set: true,
      enumerable: true,
      writable: true,
      configurable: true
    };
  for (var p in fields)
    assertEq(actual.hasOwnProperty(p), expected.hasOwnProperty(p), p);
  for (var p in actual)
    assertEq(p in fields, true, p);
  for (var p in expected)
    assertEq(p in fields, true, p);

  assertEq(actual.hasOwnProperty("value"), actual.hasOwnProperty("writable"));
  assertEq(actual.hasOwnProperty("get"), actual.hasOwnProperty("set"));
  if (actual.hasOwnProperty("value"))
  {
    assertEq(actual.value, expected.value);
    assertEq(actual.writable, expected.writable);
  }
  else
  {
    assertEq(actual.get, expected.get);
    assertEq(actual.set, expected.set);
  }

  assertEq(actual.hasOwnProperty("enumerable"), true);
  assertEq(actual.hasOwnProperty("configurable"), true);
  assertEq(actual.enumerable, expected.enumerable);
  assertEq(actual.configurable, expected.configurable);
}

function adjustDescriptorField(o, actual, expect, field)
{
  assertEq(field === "get" || field === "set", true);
  var lookup = "__lookup" + (field === "get" ? "G" : "S") + "etter";
  if (typeof o[lookup] === "function")
    expect[field] = o[lookup](field);
  else
    actual[field] = expect[field] = undefined; /* censor if we can't lookup */
}

/******************************************************************************/

var o, pd, expected;

o = { get x() { return 12; } };

pd = Object.getOwnPropertyDescriptor(o, "x");
expected =
  {
    set: undefined,
    enumerable: true,
    configurable: true
  };
adjustDescriptorField(o, pd, expected, "get");

expectDescriptor(pd, expected);

/******************************************************************************/

var o2;

o = Object.create(Object.prototype, { x: {get: function () { return 12; } } });

pd = Object.getOwnPropertyDescriptor(o, "x");
expected =
  {
    set: undefined,
    enumerable: false,
    configurable: false
  };
adjustDescriptorField(o, pd, expected, "get");

expectDescriptor(pd, expected);

o2 = Object.create(o);
assertEq(Object.getOwnPropertyDescriptor(o2, "x"), undefined);

/******************************************************************************/

o = {};
o.b = 12;

pd = Object.getOwnPropertyDescriptor(o, "b");
expected =
  {
    value: 12,
    writable: true,
    enumerable: true,
    configurable: true
  };
expectDescriptor(pd, expected);

/******************************************************************************/

o = { get y() { return 17; }, set y(z) { } };

pd = Object.getOwnPropertyDescriptor(o, "y");
expected =
  {
    enumerable: true,
    configurable: true
  };
adjustDescriptorField(o, pd, expected, "get");
adjustDescriptorField(o, pd, expected, "set");

expectDescriptor(pd, expected);

/******************************************************************************/

o = {};

pd = Object.getOwnPropertyDescriptor(o, "absent");

expectDescriptor(pd, undefined);

/******************************************************************************/

pd = Object.getOwnPropertyDescriptor([], "length");
expected =
  {
    value: 0,
    writable: true,
    enumerable: false,
    configurable: false
  };

expectDescriptor(pd, expected);

pd = Object.getOwnPropertyDescriptor([1], "length");
expected =
  {
    value: 1,
    writable: true,
    enumerable: false,
    configurable: false
  };

expectDescriptor(pd, expected);

pd = Object.getOwnPropertyDescriptor([1,], "length");
expected =
  {
    value: 1,
    writable: true,
    enumerable: false,
    configurable: false
  };

expectDescriptor(pd, expected);

pd = Object.getOwnPropertyDescriptor([1,,], "length");
expected =
  {
    value: 2,
    writable: true,
    enumerable: false,
    configurable: false
  };

expectDescriptor(pd, expected);

/******************************************************************************/

pd = Object.getOwnPropertyDescriptor(new String("foobar"), "length");
expected =
  {
    value: 6,
    writable: false,
    enumerable: false,
    configurable: false
  };

expectDescriptor(pd, expected);

/******************************************************************************/

function foo() { }
o = foo;

pd = Object.getOwnPropertyDescriptor(o, "length");
expected =
  {
    value: 0,
    writable: false,
    enumerable: false,
    configurable: false
  };

expectDescriptor(pd, expected);

pd = Object.getOwnPropertyDescriptor(o, "prototype");
expected =
  {
    value: foo.prototype,
    writable: true,
    enumerable: false,
    configurable: false
  };

expectDescriptor(pd, expected);

/******************************************************************************/

pd = Object.getOwnPropertyDescriptor(Function, "length");
expected =
  {
    value: 1,
    writable: false,
    enumerable: false,
    configurable: false
  };

expectDescriptor(pd, expected);

/******************************************************************************/

o = /foo/im;

pd = Object.getOwnPropertyDescriptor(o, "source");
expected =
  {
    value: "foo",
    writable: false,
    enumerable: false,
    configurable: false
  };

expectDescriptor(pd, expected);

pd = Object.getOwnPropertyDescriptor(o, "global");
expected =
  {
    value: false,
    writable: false,
    enumerable: false,
    configurable: false
  };

expectDescriptor(pd, expected);

pd = Object.getOwnPropertyDescriptor(o, "ignoreCase");
expected =
  {
    value: true,
    writable: false,
    enumerable: false,
    configurable: false
  };

expectDescriptor(pd, expected);

pd = Object.getOwnPropertyDescriptor(o, "multiline");
expected =
  {
    value: true,
    writable: false,
    enumerable: false,
    configurable: false
  };

expectDescriptor(pd, expected);

pd = Object.getOwnPropertyDescriptor(o, "lastIndex");
expected =
  {
    value: 0,
    writable: true,
    enumerable: false,
    configurable: false
  };

expectDescriptor(pd, expected);

/******************************************************************************/

reportCompare(expect, actual, "Object.getOwnPropertyDescriptor");

printStatus("All tests passed");
