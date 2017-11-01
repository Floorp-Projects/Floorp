/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     slice.js
   Description:  'This tests the String object method: slice'

   Author:       Nick Lerissa
   Date:         Fri Feb 13 09:58:28 PST 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var TITLE = 'String.slice';

writeHeaderToLog('Executing script: slice.js');
writeHeaderToLog( SECTION + " "+ TITLE);

var a = new String("abcdefghijklmnopqrstuvwxyz1234567890");
var b = new String("this is a test string");

exhaustiveStringSliceTest("exhaustive String.slice test 1", a);
exhaustiveStringSliceTest("exhaustive String.slice test 2", b);

test();


function myStringSlice(a, from, to)
{
  var from2        = from;
  var to2          = to;
  var returnString = new String("");
  var i;

  if (from2 < 0) from2 = a.length + from;
  if (to2 < 0)   to2   = a.length + to;

  if ((to2 > from2)&&(to2 > 0)&&(from2 < a.length))
  {
    if (from2 < 0)        from2 = 0;
    if (to2 > a.length) to2 = a.length;

    for (i = from2; i < to2; ++i) returnString += a.charAt(i);
  }
  return returnString;
}

// This function tests the slice command on a String
// passed in. The arguments passed into slice range in
// value from -5 to the length of the array + 4. Every
// combination of the two arguments is tested. The expected
// result of the slice(...) method is calculated and
// compared to the actual result from the slice(...) method.
// If the Strings are not similar false is returned.
function exhaustiveStringSliceTest(testname, a)
{
  var x = 0;
  var y = 0;
  var errorMessage;
  var reason = "";
  var passed = true;

  for (x = -(2 + a.length); x <= (2 + a.length); x++)
    for (y = (2 + a.length); y >= -(2 + a.length); y--)
    {
      var b  = a.slice(x,y);
      var c = myStringSlice(a,x,y);

      if (String(b) != String(c))
      {
	errorMessage =
	  "ERROR: 'TEST FAILED' ERROR: 'TEST FAILED' ERROR: 'TEST FAILED'\n" +
	  "            test: " + "a.slice(" + x + "," + y + ")\n" +
	  "               a: " + String(a) + "\n" +
	  "   actual result: " + String(b) + "\n" +
	  " expected result: " + String(c) + "\n";
	writeHeaderToLog(errorMessage);
	reason = reason + errorMessage;
	passed = false;
      }
    }

  new TestCase(testname, true, passed, reason);
}
