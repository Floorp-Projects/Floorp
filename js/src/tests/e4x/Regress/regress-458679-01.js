// |reftest| pref(javascript.options.xml.content,true) skip-if(Android) silentfail
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = 'GetXMLEntity should not assume FastAppendChar is infallible';
var BUGNUMBER = 458679;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

expectExitCode(0);
expectExitCode(5);

try
{
    var x = "<";

    while (x.length < 12000000)
        x += x;

    <e4x>{x}</e4x>;
}
catch(ex)
{
}

TEST(1, expect, actual);

END();
