/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    16 September 2002
 * SUMMARY: Testing propertyIsEnumerable() on nonexistent property
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=156354
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 156354;
var summary = 'Testing propertyIsEnumerable() on nonexistent property';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


status = inSection(1);
actual = this.propertyIsEnumerable('XYZ');
expect = false;
addThis();

status = inSection(2);
actual = this.propertyIsEnumerable('');
expect = false;
addThis();

status = inSection(3);
actual = this.propertyIsEnumerable(undefined);
expect = false;
addThis();

status = inSection(4);
actual = this.propertyIsEnumerable(null);
expect = false;
addThis();

status = inSection(5);
actual = this.propertyIsEnumerable('\u02b1');
expect = false;
addThis();


var obj = {prop1:null};

status = inSection(6);
actual = obj.propertyIsEnumerable('prop1');
expect = true;
addThis();

status = inSection(7);
actual = obj.propertyIsEnumerable('prop2');
expect = false;
addThis();

// let's try one via eval(), too -
status = inSection(8);
eval("actual = obj.propertyIsEnumerable('prop2')");
expect = false;
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
