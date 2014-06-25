/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    26 September 2003
 * SUMMARY: Regexp conformance test
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=220367
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 220367;
var summary = 'Regexp conformance test';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];

var re = /(a)|(b)/;

re.test('a');
status = inSection(1);
actual = RegExp.$1;
expect = 'a';
addThis();

status = inSection(2);
actual = RegExp.$2;
expect = '';
addThis();

re.test('b');
status = inSection(3);
actual = RegExp.$1;
expect = '';
addThis();

status = inSection(4);
actual = RegExp.$2;
expect = 'b';
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
  enterFunc('test');
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
