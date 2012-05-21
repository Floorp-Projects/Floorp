/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 
jit(true);

this.watch('x', Math.pow); (function() { for(var j=0;j<4;++j){x=1;} })();

jit(false);

reportCompare(expect, actual, summary);
