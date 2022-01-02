// |reftest| skip-if(Android&&isDebugBuild) slow -- bug 1216226
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 360969;
var summary = '2^17: global function';
var actual = 'No Crash';
var expect = 'No Crash';

var global = this;

printBugNumber(BUGNUMBER);
printStatus (summary);
 
var start = new Date();
var p;
var i;
var limit = 2 << 16;

for (var i = 0; i < limit; i++)
{
  eval('function pf' + i + '() {}');
}

reportCompare(expect, actual, summary);

var stop = new Date();

print('Elapsed time: ' + Math.floor((stop - start)/1000) + ' seconds');

