/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 381304;
var summary = 'getter/setter with keywords';
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
 
  var obj;

  print('1');

  obj = {
    set inn(value) {this.for = value;}, 
    get inn() {return this.for;}
  };

  expect = '({get inn() { return this.for; }, set inn(value) { this.for = value; } })';
  actual = obj.toSource();
  compareSource(expect, actual, summary + ': 1');

  print('2');

  obj = {
    set in(value) {this.for = value;}, 
    get in() {return this.for;}
  };

  expect = '({ get in() { return this.for; }, set in(value) { this.for = value; } })';
  actual = obj.toSource();
  compareSource(expect, actual, summary + ': 2');

  print('3');

  obj = {
    set inn(value) {this.for = value;}, 
    get in() {return this.for;}
  };

  expect = '({ set inn(value) { this.for = value; }, get in() { return this.for; } })';
  actual = obj.toSource();
  compareSource(expect, actual, summary + ': 4');

  print('4');

  obj = {
    set in(value) {this.for = value;}, 
    get inn() {return this.for;}
  };

  expect = ' ({ set in(value) { this.for = value; }, get inn() { return this.for; } })';
  actual = obj.toSource();
  compareSource(expect, actual, summary + ': 5');
  exitFunc ('test');
}
