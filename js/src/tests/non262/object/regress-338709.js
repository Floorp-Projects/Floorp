/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 338709;
var summary = 'ReadOnly properties should not be overwritten by using ' +
  'Object and try..throw..catch';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

Object = function () { return Math };
expect = Math.LN2;
try
{
  throw 1990;
}
catch (LN2)
{
}
actual = Math.LN2;
print("Math.LN2 = " + Math.LN2)
  reportCompare(expect, actual, summary);

var s = new String("abc");
Object = function () { return s };
expect = s.length;
try
{
  throw -8
    }
catch (length)
{
}
actual = s.length;
print("length of '" + s + "' = " + s.length)
  reportCompare(expect, actual, summary);

var re = /xy/m;
Object = function () { return re };
expect = re.multiline;
try
{
  throw false
    }
catch (multiline)
{
}
actual = re.multiline;
print("re.multiline = " + re.multiline)
  reportCompare(expect, actual, summary);

if ("document" in this) {
  // Let the document be its own documentElement.
  Object = function () { return document }
  expect = document.documentElement + '';
  try
  {
    throw document;
  }
  catch (documentElement)
  {
  }
  actual = document.documentElement + '';
  print("document.documentElement = " + document.documentElement)
    }
else
  Object = this.constructor
 
    reportCompare(expect, actual, summary);
