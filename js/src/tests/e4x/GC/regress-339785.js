/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "scanner: memory exposure to scripts";
var BUGNUMBER = 339785;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

function evalXML(N)
{
    var str = Array(N + 1).join('a'); // str is string of N a
    src = "var x = <xml>&"+str+";</xml>;";
    try {
        eval(src);
        return "Should have thrown unknown entity error";
    } catch (e) {
        return e.message;
    }
    return "Unexpected";
}

var N1 = 1;
var must_be_good = evalXML(N1);
expect = 'unknown XML entity a';
actual = must_be_good;
TEST(1, expect, actual);

function testScanner()
{
    for (var power = 2; power != 15; ++power) {
        var N2 = (1 << power) - 2;
        var can_be_bad  = evalXML(N2);
        var diff = can_be_bad.length - must_be_good.length;
        if (diff != 0 && diff != N2 - N1) {
            return "Detected memory exposure at entity length of "+(N2+2);
        }
    }
    return "Ok";
}

expect = "Ok";

// repeat test since test does not always fail

for (var iTestScanner = 0; iTestScanner < 100; ++iTestScanner)
{
    actual = testScanner();
    TEST(iTestScanner+1, expect, actual);
}


END();
