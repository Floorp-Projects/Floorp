/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 306591;
var summary = 'String static methods';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
printStatus ('See https://bugzilla.mozilla.org/show_bug.cgi?id=304828');
 
expect = ['a', 'b', 'c'].toString();
actual = String.split(new String('abc'), '').toString();
reportCompare(expect, actual, summary +
              " String.split(new String('abc'), '')");

expect = '2';
actual = String.substring(new Number(123), 1, 2);
reportCompare(expect, actual, summary +
              " String.substring(new Number(123), 1, 2)");

expect = 'TRUE';
actual = String.toUpperCase(new Boolean(true)); 
reportCompare(expect, actual, summary +
              " String.toUpperCase(new Boolean(true))");

try
{
  String.indexOf(null, 'l');
  throw new Error("should have thrown a TypeError");
}
catch (e)
{
  assertEq(e instanceof TypeError, true,
           "String.indexOf(null [, ...]) didn't work correctly");
}

expect = 2;
actual = String.indexOf(String(null), 'l');             
reportCompare(expect, actual, summary +
              " String.indexOf(String(null), 'l')");

expect = ['a', 'b', 'c'].toString();
actual = String.split('abc', '').toString();
reportCompare(expect, actual, summary +
              " String.split('abc', '')");

expect = '2';
actual = String.substring(123, 1, 2);
reportCompare(expect, actual, summary +
              " String.substring(123, 1, 2)");

expect = 'TRUE';
actual = String.toUpperCase(true);
reportCompare(expect, actual, summary +
              " String.toUpperCase(true)");

try
{
  String.indexOf(undefined, 'd');
  throw new Error("should have thrown a TypeError");
}
catch (e)
{
  assertEq(e instanceof TypeError, true,
           "String.indexOf(undefined [, ...]) didn't work correctly");
}

expect = 2;
actual = String.indexOf(String(undefined), 'd');
reportCompare(expect, actual, summary +
              " String.indexOf(String(undefined), 'd')");
