// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "11.1.4 - ]] should be allowed in CDATA Section";
var BUGNUMBER = 313929;
var actual = 'No error';
var expect = 'No error';

printBugNumber(BUGNUMBER);
START(summary);

try
{
    actual = XML("<x><![CDATA[ ]] ]]></x>").toString();
}
catch(e)
{
    actual = e + '';
}

expect = (<x> ]] </x>).toString();

TEST(1, expect, actual);

END();
