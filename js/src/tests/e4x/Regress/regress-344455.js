// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "Error - tag name mismatch error message should include tag name";
var BUGNUMBER = 344455;
var actual = '';
var expect = 'SyntaxError: XML tag name mismatch (expected foo)';

printBugNumber(BUGNUMBER);
START(summary);

try
{
    eval('x = <foo></bar>;');
}
catch(ex)
{
    actual = ex + '';
}

TEST(1, expect, actual);

END();
