// |reftest| skip -- slow
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "Do not crash creating XML object with long initialiser";
var BUGNUMBER = 324422;
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);
printStatus ("Expect out of memory error");

expectExitCode(0);
expectExitCode(5);

try
{
    var str = '0123456789';

    for (var icount = 0; icount < 24; icount++)
    {
        str = str + str;
    }

    printStatus(str.length);

    var x = new XML('<root>' + str + '</root>');
}
catch(ex)
{
    print(ex + '');
}
TEST(1, expect, actual);

END();
