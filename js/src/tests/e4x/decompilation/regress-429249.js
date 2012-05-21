// |reftest| require-or(debugMode,skip)
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = 'trap should not change decompilation <x/>';
var BUGNUMBER = 429249
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

function g() {
    return <x/>;
}

expect = 'function g() { return <x/>; }';
actual = g + '';
compareSource(expect, actual, summary + ' : before trap');

if (typeof trap == 'function' && typeof setDebug == 'function')
{
    setDebug(true);
    trap(g, 0, "");
    actual = g + '';
    compareSource(expect, actual, summary + ' : after trap');
}

END();
