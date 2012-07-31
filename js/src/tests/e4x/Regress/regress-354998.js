// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 354998;
var summary = 'prototype should not be enumerated for XML objects.';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

function test()
{
    var list = <><a/><b/></>;
    var count = 0;
    var now = Date.now;
    var time = now();
    for (var i in list) {
        ++count;
    }
    time = now() - time;
    if (count != 2) {
        if (count < 2)
            throw "Enumerator has not looped over all properties, count="+count;
        throw "Enumerator has looped over prototype chain of xml, count="+count;
    }
    return time;
}

try
{
    var time1 = test();

    for (var i = 0; i != 1000*1000; ++i)
        Object.prototype[i] = i;

    var time2 = test();

    // clean up Object prototype so it won't
    // hang enumerations in options()...

    for (var i = 0; i != 1000*1000; ++i)
        delete Object.prototype[i];

    if (time1 * 10 + 1 < time2) {
        throw "Assigns to Object.prototype increased time of XML enumeration from "+
            time1+"ms to "+time2+"ms";
    }
}
catch(ex)
{
    actual = ex = '';
}

TEST(1, expect, actual);

END();
