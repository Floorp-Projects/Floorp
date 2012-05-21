/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER = 381303;
var summary = 'object toSource when a property has both a getter and a setter';
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
 
  var obj = {set inn(value) {this.for = value;}, get inn() {return this.for;}};
  expect = '( { ' + 
    'get inn() {return this.for;}' + 
    ', ' + 
    'set inn(value) {this.for = value;}' + 
    '})';
  actual = obj.toSource();

  compareSource(expect, actual, summary);

  exitFunc ('test');
}
