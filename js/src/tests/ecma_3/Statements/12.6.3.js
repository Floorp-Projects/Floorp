/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 292731;
var summary = 'for-in should not call valueOf method';
var actual = '';
var expect = '';
var i;

printBugNumber(BUGNUMBER);
printStatus (summary);

function MyObject()
{
}

MyObject.prototype.valueOf = function()
{
  actual += 'MyObject.prototype.valueOf called. ';
}

  var myobject = new MyObject();

var myfunction = new function()
{
  this.valueOf = function()
  {
    actual += 'this.valueOf called. ';
  }
}

  actual = '';
for (i in myobject)
{
  //calls valueOf
}
reportCompare(expect, actual, 'for-in custom object');

actual = '';
for (i in myfunction)
{
  //calls valueOf
}
reportCompare(expect, actual, 'for-in function expression');
