/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 167328;
var summary = 'Normal error reporting code should fill Error object properties';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

expect = 'TypeError:19';
try
{
  var obj = {toString: function() {return new Object();}};
  var result = String(obj);
  actual = 'no error';
}
catch(e)
{
  actual = e.name + ':' + e.lineNumber;
} 
reportCompare(expect, actual, summary);
