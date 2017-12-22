/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 469937;
var summary = 'Properties without DontEnum are sometimes not enumerated';
var actual = false;
var expect = true;

printBugNumber(BUGNUMBER);
printStatus (summary);
 
(function(){ 
    var o = { }
    o.PageLeft = 1;
    o.Rect2 = 6;
    delete o.Rect2;
    for (var p in o);
    o.Rect3 = 7;
    found = false;
    for (var p in o) if (p == 'Rect3') found = true;
    actual = found;    
})();

reportCompare(expect, actual, summary);
