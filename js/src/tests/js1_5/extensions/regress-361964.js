// |reftest| skip -- slow, alert not dismissed, now busted by harness
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 361964;
var summary = 'Crash [@ MarkGCThingChildren] involving watch and setter';
var actual = 'No Crash';
var expect = 'No Crash';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  var doc;
  if (typeof document == 'undefined')
  {
    doc = {};
  }
  else
  {
    doc = document;
  }

  if (typeof alert == 'undefined')
  {
    alert = print;
  }

// Crash:
  doc.watch("title", function(a,b,c,d) {
		   return { toString : function() { alert(1); } };
		 });
  doc.title = "xxx";

// No crash:
  doc.watch("title", function() {
		   return { toString : function() { alert(1); } };
		 });
  doc.title = "xxx";

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
