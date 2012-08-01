// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "Crash due to infinite recursion in js_IsXMLName";
var BUGNUMBER = 328249;
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

try
{
    var A = <x/>;
    var B = A.p1;
    var C = B.p2;
    B.p3 = C;
    C.p4 = B;
    C.appendChild(B);
    C.p5 = C;
}
catch(ex)
{
    printStatus(ex+'');
}
TEST(1, expect, actual);

END();
