// |reftest| skip-if(!this.uneval)

/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 452498;
var summary = 'TM: upvar2 regression tests';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

// ------- Comment #101 From Gary Kwong [:nth10sd]

  uneval(function(){with({functional: []}){x5, y = this;const y = undefined }});
// Assertion failure: strcmp(rval, with_cookie) == 0, at ../jsopcode.cpp:2567

  reportCompare(expect, actual, summary);
}
