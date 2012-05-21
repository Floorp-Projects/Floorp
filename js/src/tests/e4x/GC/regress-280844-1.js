// |reftest| skip-if(Android)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = 'Uncontrolled recursion in js_MarkXML during GC';
var BUGNUMBER = 280844;
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

var N = 5 * 1000;
var x = <x/>;
for (var i = 1; i <= N; ++i) {
    x.appendChild(<x/>);
    x = x.x[0];
}
printStatus(x.toXMLString());
gc();

TEST(1, expect, actual);
END();
