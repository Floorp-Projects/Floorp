/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "11.1.4 - XML Initializer should accept single CDATA Section";
var BUGNUMBER = 257679;
var actual = '';
var expect = 'text';

printBugNumber(BUGNUMBER);
START(summary);

var cdataText = <![CDATA[Kibology for all.<br>All for Kibology.]]>;
if (cdataText) {
    actual = cdataText.nodeKind();
}
else {
    actual = 'undefined';
}

TEST(1, expect, actual);

END();
