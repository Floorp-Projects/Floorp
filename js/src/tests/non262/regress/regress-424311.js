/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 424311;
var summary = 'Do not assert: entry->kpc == ((PCVCAP_TAG(entry->vcap) > 1) ? (jsbytecode *) JSID_TO_ATOM(id) : cx->fp->regs->pc)';
var actual = 'No Crash';
var expect = 'No Crash';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  (function(){(function(){ constructor=({}); })()})();

  reportCompare(expect, actual, summary);
}
