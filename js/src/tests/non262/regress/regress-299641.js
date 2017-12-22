/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 299641;
var summary = '12.6.4 - LHS for (LHS in expression) execution';
var actual = '';
var expect = 0;

printBugNumber(BUGNUMBER);
printStatus (summary);

function f() {
  var i = 0;
  var a = [{x: 42}];
  for (a[i++].x in [])
    return(a[i-1].x);
  return(i);
}
actual = f();
 
reportCompare(expect, actual, summary);
