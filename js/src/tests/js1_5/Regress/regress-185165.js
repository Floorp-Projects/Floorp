/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    13 Dec 2002
 * SUMMARY: Decompilation of "\\" should give "\\"
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=185165
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 185165;
var summary = 'Decompilation of "\\\\" should give "\\\\"';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


// Check that second decompilation of script gives the same string as first one
var f1 = function() { return "\\"; }
  var s1 = f1.toString();

var f2;
eval("f2=" + s1);
var s2 = f2.toString();

status = inSection(1);
actual = s2;
expect = s1;
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
