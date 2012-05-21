/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "11.1.1 - Attribute Identifiers Do not crash when " +
    "attribute-op name collides with local var";
var BUGNUMBER = 301545;
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

function TOCParser(aElement) {
  var href = aElement.@href;
}

TEST(summary, expect, actual);

END();
