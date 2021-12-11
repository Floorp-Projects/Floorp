/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 280769;
var summary = 'Do not crash on overflow of 64K boundary of [] offset in regexp search string ';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

status = summary + ' ' + inSection(1) + ' (new RegExp("zzz...[AB]").exec("zzz...A") ';

var N = 100 * 1000; // N should be more then 64K
var a = new Array(N + 1);
var prefix = a.join("z"); // prefix is sequence of N "z", zzz...zzz
var str = prefix+"[AB]"; // str is zzz...zzz[AB]
var re = new RegExp(str);
try { 
  re.exec(prefix+"A");
  reportCompare(expect, actual, status);
} catch (e) {
  reportCompare(true, e instanceof Error, actual, status);
}
