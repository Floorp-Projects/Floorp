/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 350256;
var summary = 'Array.apply maximum arguments';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
reportCompare(true, true, "");

//-----------------------------------------------------------------------------
if (this.getMaxArgs)
    test(getMaxArgs());

//-----------------------------------------------------------------------------

function test(length)
{
  var a = new Array();
  a[length - 2] = 'length-2';
  a[length - 1] = 'length-1';

  var b = Array.apply(null, a);

  expect = length + ',length-2,length-1';
  actual = b.length + "," + b[length - 2] + "," + b[length - 1];
  reportCompare(expect, actual, summary);

  function f() {
    return arguments.length + "," + arguments[length - 2] + "," +
      arguments[length - 1];
  }

  expect = length + ',length-2,length-1';
  actual = f.apply(null, a);

  reportCompare(expect, actual, summary);
}
