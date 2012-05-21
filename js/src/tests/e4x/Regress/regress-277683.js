/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


// testcase from  Martin.Honnen@arcor.de

var summary = 'processing instruction with target name XML';
var BUGNUMBER = 277683;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

expect = 'error';
try
{
    XML.ignoreProcessingInstructions = false;
    var xml = <root><child>Kibo</child><?xml version="1.0"?></root>;
    actual = 'no error';
}
catch(e)
{
    actual = 'error';
}

TEST(1, expect, actual);

END();
