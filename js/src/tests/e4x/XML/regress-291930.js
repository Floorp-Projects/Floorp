/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("If document starts with comment, document is discarded");
printBugNumber(291930);

XML.ignoreComments = false;
try {
	var root = new XML("<!-- Sample --> <root/>");
	SHOULD_THROW(1, "SyntaxError");
} catch (e) {
	TEST(1, "error", "error");
}

END();
