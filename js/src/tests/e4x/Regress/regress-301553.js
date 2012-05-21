/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "E4X - Should not repress exceptions";
var BUGNUMBER = 301553;
var actual = 'No exception';
var expect = 'exception';

printBugNumber(BUGNUMBER);
START(summary);

try
{
    var x = <xml/>;
    Object.toString.call(x);
}
catch(e)
{
    actual = 'exception';
}

TEST(1, expect, actual);
END();
