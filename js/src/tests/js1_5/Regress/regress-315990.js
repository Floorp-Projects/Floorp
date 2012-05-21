/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 315990;
var summary = 'this.statement.is.an.error';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary + ': function expression');

expect = 'TypeError';
try
{
  (function() {
    this.statement.is.an.error;
  })()
    }
catch(ex)
{
  printStatus(ex);
  actual = ex.name;
} 
reportCompare(expect, actual, summary + ': function expression');


printStatus (summary + ': top level');
try
{
  this.statement.is.an.error;
}
catch(ex)
{
  printStatus(ex);
  actual = ex.name;
} 
reportCompare(expect, actual, summary + ': top level');
