/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is SpiderMonkey ES5 tests.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jeff Walden <jwalden+code@mit.edu> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var gTestfile = '15.2.3.3-01.js';
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

o = { get y() { return 17; }, set y() { } };

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

printStatus("All tests passed");
