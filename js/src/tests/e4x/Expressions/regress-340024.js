/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 340024;
var summary = '11.1.4 - XML Initializer';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

expect = '<tag b="c" d="e"/>';
try
{
    actual = (<tag {0?"a":"b"}="c" d="e"/>.toXMLString());
}
catch(E)
{
    actual = E + '';
}

TEST(1, expect, actual);

END();
