/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    02 July 2003
 * SUMMARY: testing line ending with |continue| and only terminated by a CR
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=210682
 *
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 210682;
var summary = 'testing line ending with |continue| and only terminated by CR';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


for (i=0; i<100; i++)
{
  if (i%2 == 0) continue
    this.lasti = i;
}

status = inSection(1);
actual = lasti;
expect = 99;
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
