/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 496922;
var summary = 'Incorrect handling of extra arguments';
var actual = ''
var expect = '0,0,1,1,2,2,3,3';


//-----------------------------------------------------------------------------

// The code must run as part of the top-level script in order to get the bug.
printBugNumber(BUGNUMBER);
printStatus (summary);

var a = [];
{
let f = function() {
    for (let x = 0; x < 4; ++x) {
        (function() {
            for (let y = 0; y < 2; ++y) {
              a.push(x);
            }
        })()
    }
}; (function() {})()
    f(99)
}
actual = '' + a;

reportCompare(expect, actual, summary);
//-----------------------------------------------------------------------------

