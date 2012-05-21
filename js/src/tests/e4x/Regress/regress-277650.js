/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


// testcase from  Martin.Honnen@arcor.de

var summary = 'xml:lang attribute in XML literal';
var BUGNUMBER = 277650;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

expect = 'no error';
try
{
    var xml = <root><text xml:lang="en">ECMAScript for XML</text></root>;
    actual = 'no error';
}
catch(e)
{
    actual = 'error';
}

TEST(1, expect, actual);

END();
