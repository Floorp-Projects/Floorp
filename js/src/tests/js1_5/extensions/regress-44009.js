/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 26 Feb 2001
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=44009
 *
 * SUMMARY:  Testing that we don't crash on obj.toSource()
 */
//-----------------------------------------------------------------------------
var BUGNUMBER = 44009;
var summary = "Testing that we don't crash on obj.toSource()";
var obj1 = {};
var sToSource = '';
var self = this;  //capture a reference to the global JS object -



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  var obj2 = {};

  // test various objects and scopes -
  testThis(self);
  testThis(this);
  testThis(obj1);
  testThis(obj2);

  reportCompare('No Crash', 'No Crash', '');

  exitFunc ('test');
}


// We're just testing that we don't crash by doing this -
function testThis(obj)
{
  sToSource = obj.toSource();
  obj.prop = obj;
  sToSource = obj.toSource();
}
