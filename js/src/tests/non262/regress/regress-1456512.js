/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var BUGNUMBER = 1456512;
var summary = 'rogue read barrier';

printBugNumber(BUGNUMBER);
printStatus (summary);

var wm = new WeakMap();
grayRoot().map = wm;
wm = null;
gczeal(13, 7);
var lfOffThreadGlobal = newGlobal();

if (typeof reportCompare == 'function')
    reportCompare(true, true, "ok");
