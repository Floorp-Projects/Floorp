/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = 'Do not crash with XML.prettyIndent = 2147483648';
var BUGNUMBER = 374025;
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

XML.prettyIndent = 2147483648; 
uneval(<x><y/></x>);

TEST(1, expect, actual);

END();
