/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 16 May 2001
 *
 * SUMMARY: Regression test for bug 76054
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=76054
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=78706
 * All String HTML methods should be LOWER case -
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 76054;
var summary = 'Testing that String HTML methods produce all lower-case';
var statprefix = 'Currently testing String.';
var status = '';
var statusitems = [ ];
var actual = '';
var actualvalues = [ ];
var expect= '';
var expectedvalues = [ ];
var s = 'xyz';

status = 'anchor()';
actual = s.anchor();
expect = actual.toLowerCase();
addThis();

status = 'big()';
actual = s.big();
expect = actual.toLowerCase();
addThis();

status = 'blink()';
actual = s.blink();
expect = actual.toLowerCase();
addThis();

status = 'bold()';
actual = s.bold();
expect = actual.toLowerCase();
addThis();

status = 'italics()';
actual = s.italics();
expect = actual.toLowerCase();
addThis();

status = 'fixed()';
actual = s.fixed();
expect = actual.toLowerCase();
addThis();

status = 'fontcolor()';
actual = s.fontcolor();
expect = actual.toLowerCase();
addThis();

status = 'fontsize()';
actual = s.fontsize();
expect = actual.toLowerCase();
addThis();

status = 'link()';
actual = s.link();
expect = actual.toLowerCase();
addThis();

status = 'small()';
actual = s.small();
expect = actual.toLowerCase();
addThis();

status = 'strike()';
actual = s.strike();
expect = actual.toLowerCase();
addThis();

status = 'sub()';
actual = s.sub();
expect = actual.toLowerCase();
addThis();

status = 'sup()';
actual = s.sup();
expect = actual.toLowerCase();
addThis();


//-------------------------------------------------------------------------------------------------
test();
//-------------------------------------------------------------------------------------------------


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
  printStatus (summary);

  for (var i = 0; i < UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], getStatus(i));
  }
}


function getStatus(i)
{
  return statprefix + statusitems[i];
}
