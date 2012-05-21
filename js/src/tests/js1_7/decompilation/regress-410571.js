/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 410571;
var summary = 'incorrect decompilation of last element of object literals';
var actual, expect;

actual =  expect = 'PASSED';

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

function getProps(o)
{
  var props = [];
  for (var i in o)
    props.push(i);
  return props.sort().join(",");
}

var tests =
  [
   {
    fun: function()
    {
      yield { x: 1, y: let (z = 7) z };
    },
    generates:
      [
       function(rv)
       {
         return typeof rv === "object" &&
                rv.x === 1 &&
                rv.y === 7 &&
                getProps(rv) === "x,y";
       },
      ]
   },
   {
    fun: function()
     {
       var t = { x: 3, y: yield 4 };
       yield t;
     },
     generates:
       [
        function(rv) { return rv == 4; },
        function(rv)
        {
          return typeof rv === "object" &&
                 rv.x === 3 &&
                 rv.y === undefined &&
                 getProps(rv) === "x,y";
        },
       ]
   },
   {
    fun: function()
    {
      var t = { x: 3, get y() { return 17; } };
      yield t;
    },
    generates:
      [
       function(rv)
       {
         return typeof rv === "object" &&
                rv.x === 3 &&
                rv.y === 17 &&
                getProps(rv) === "x,y";
       }
      ]
   },
  ];

function checkItems(name, gen)
{
  var i = 0;
  for (var item in gen)
  {
    if (!test.generates[i](item))
      actual = "wrong generated value (" + item + ") " +
            "for test " + name + ", item " + i;
    i++;
  }
  if (i !== test.generates.length)
    actual = "Didn't iterate all of test " + name;
}

for (var i = 0, sz = tests.length; i < sz; i++)
{
  var test = tests[i];

  var fun = test.fun;
  checkItems(i, fun());

  var dec = '(' + fun.toString() + ')';
  var rec = eval(dec);
  checkItems("recompiled " + i, rec());
}

reportCompare(expect, actual, summary);
