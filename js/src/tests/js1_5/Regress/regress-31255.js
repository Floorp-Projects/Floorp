/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    09 November 2002
 * SUMMARY: JS should treat --> as a single-line comment indicator.
 *          Whitespace may occur before the --> on the same line.
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=31255
 * and http://bugzilla.mozilla.org/show_bug.cgi?id=179366 (Rhino version)
 *
 * Note: <!--, --> are the HTML multi-line comment opener, closer.
 * JS already accepted <!-- as a single-line comment indicator.
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 31255;
var summary = 'JS should treat --> as a single-line comment indicator';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


<!-- HTML comment start is already a single-line JS comment indicator
 var x = 1; <!-- until end-of-line

	     status = inSection(1);
actual = (x == 1);
expect = true;
addThis();

--> HTML comment end is JS comments until end-of-line
--> but only if it follows a possible whitespace after line start
--> so in the following --> should not be treated as comments
if (x-->0)
  x = 2;

status = inSection(2);
actual = (x == 2);
expect = true;
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}


function test()
{
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }
}
