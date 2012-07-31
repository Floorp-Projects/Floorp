// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "11.1.4 - XML Initializer - <p:{b}b>x</p:bb>";
var BUGNUMBER = 321549;
var actual = 'No error';
var expect = 'No error';

printBugNumber(BUGNUMBER);
START(summary);

var b = 'b';

try
{
    actual = (<a xmlns:p='http://a.uri/'><p:b{b}>x</p:bb></a>).
        toString();
}
catch(e)
{
    actual = e + '';
}

expect = (<a xmlns:p='http://a.uri/'><p:bb>x</p:bb></a>).toString();

TEST(1, expect, actual);

try
{
    actual = (<a xmlns:p='http://a.uri/'><p:{b}b>x</p:bb></a>).
        toString();
}
catch(e)
{
    actual = e + '';
}

expect = (<a xmlns:p='http://a.uri/'><p:bb>x</p:bb></a>).toString();

TEST(2, expect, actual);

END();
