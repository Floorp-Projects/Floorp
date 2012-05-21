/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 246964;
// see also bug 248549, bug 253150, bug 259935
var summary = 'undetectable document.all';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

if (typeof document == 'undefined')
{
  expect = actual = 'Test requires browser: skipped';
  reportCompare(expect, actual, summary);
}
else
{
  status = summary + ' ' + inSection(1) + ' if (document.all) ';
  expect = false;
  actual = false;
  if (document.all)
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(2) + 'if (isIE) ';
  expect = false;
  actual = false;
  var isIE = document.all;
  if (isIE)
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(3) + ' if (document.all != undefined) ';
  expect = false;
  actual = false;
  if (document.all != undefined)
  {
    actual = true;
  }
  reportCompare(expect, actual, status);


  status = summary + ' ' + inSection(4) + ' if (document.all !== undefined) ';
  expect = false;
  actual = false;
  if (document.all !== undefined)
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(5) + ' if (document.all != null) ' ;
  expect = false;
  actual = false;
  if (document.all != null)
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(6) + ' if (document.all !== null) ' ;
  expect = true;
  actual = false;
  if (document.all !== null)
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(7) + ' if (document.all == null) ';
  expect = true;
  actual = false;
  if (document.all == null)
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(8) + ' if (document.all === null) ';
  expect = false;
  actual = false;
  if (document.all === null)
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(9) + ' if (document.all == undefined) ';
  expect = true;
  actual = false;
  if (document.all == undefined)
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(10) + ' if (document.all === undefined) ';
  expect = true;
  actual = false;
  if (document.all === undefined)
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(11) +
    ' if (typeof document.all == "undefined") ';
  expect = true;
  actual = false;
  if (typeof document.all == 'undefined')
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(12) +
    ' if (typeof document.all != "undefined") ';
  expect = false;
  actual = false;
  if (typeof document.all != 'undefined')
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(13) + ' if ("all" in document) ';
  expect = (document.compatMode == 'CSS1Compat') ? false : true;
  actual = false;
  if ('all' in document)
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(14) + ' if (f.ie) ';
  var f = new foo();

  expect = false;
  actual = false;
  if (f.ie)
  {
    actual = true;
  }
  reportCompare(expect, actual, status);

}

function foo()
{
  this.ie = document.all;
}
