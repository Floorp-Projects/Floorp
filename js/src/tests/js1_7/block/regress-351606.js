/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 351606;
var summary = 'Do not assert with nested for..in and throw';
var actual = 'No Crash';
var expect = 'No Crash';

enterFunc ('test');
printBugNumber(BUGNUMBER);
printStatus (summary);

(function () {for (let d in [1,2,3,4]) try { for (let a in [5,6,7,8]) (( function() { throw 9; } )()); } catch(c) {  }});

reportCompare(expect, actual, summary);


