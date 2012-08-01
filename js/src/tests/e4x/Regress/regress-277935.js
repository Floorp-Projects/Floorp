// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START('XML("") should create empty text node');
printBugNumber(277935);

// Check that assignments like "a..b = c" causes proper error

var expect = 'PASS1,PASS2';
var actual = '';

var msg =  <foo><bar><s/></bar></foo>;

try {
    eval('msg..s = 0');
    SHOULD_THROW(1);
} catch (e) {
    actual += 'PASS1';
}

try {
    eval('msg..s += 0');
    SHOULD_THROW(2);
} catch (e) {
    actual += ',PASS2';
}

TEST(1, expect, actual);
END();
