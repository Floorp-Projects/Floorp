/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER = 379566;
var summary = 'Keywords after get|set';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  expect = '({ ' +
    'get in () { return this.for; }, ' + 
    'set in (value) { this.for = value; } ' + 
    '})';
  try
  {
    var obj = eval('({ ' +
                   'get in() { return this.for; }, ' + 
                   'set in(value) { this.for = value; } ' + 
                   '})');
    actual = obj.toSource();

  }
  catch(ex)
  {
    actual = ex + '';
  }

  compareSource(expect, actual, summary);

  exitFunc ('test');
}
