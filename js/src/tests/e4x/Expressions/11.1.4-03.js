/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = '11.1.4 - XML Initializer should accept single comment';
var BUGNUMBER = 257679;
var actual = '';
var expect = 'comment';

printBugNumber(BUGNUMBER);
START(summary);

XML.ignoreComments = false;
print("XML.ignoreComments: " + XML.ignoreComments);
var comment = <!-- comment -->;
if (comment) {
    actual = comment.nodeKind();
}
else {
    actual = 'undefined';
}

TEST(1, expect, actual);

END();
