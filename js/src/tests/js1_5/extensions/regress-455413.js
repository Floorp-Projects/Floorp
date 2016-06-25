/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 455413;
var summary = 'Do not assert with JIT: (m != JSVAL_INT) || isInt32(*vp)';
var actual = 'No Crash';
var expect = 'No Crash';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

printBugNumber(BUGNUMBER);
printStatus (summary);
 

this.watch('x', Math.pow); (function() { for(var j=0;j<4;++j){x=1;} })();


reportCompare(expect, actual, summary);
