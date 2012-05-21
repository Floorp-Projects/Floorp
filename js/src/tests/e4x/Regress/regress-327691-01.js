/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "Do not crash in js_IsXMLName";
var BUGNUMBER = 327691;
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

var A=<y/>;
var B=A.h;
var D=B.h;
B.h=D.h;
B[0]=B.h;

B.h=D.h; // Crashes at js_IsXMLName.

TEST(1, expect, actual);

END();
