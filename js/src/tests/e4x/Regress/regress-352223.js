// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 352223;
var summary = 'Reject invalid spaces in tags';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

expect = 'SyntaxError: invalid XML name';
try
{
    eval('< foo></foo>');
}
catch(ex)
{
    actual = ex + '';
}
TEST(1, expect, actual);

expect = 'SyntaxError: invalid XML tag syntax';
try
{
    eval('<foo></ foo>');
}
catch(ex)
{
    actual = ex + '';
}
TEST(2, expect, actual);

END();
