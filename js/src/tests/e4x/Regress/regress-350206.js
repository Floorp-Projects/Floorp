/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 350206;
var summary = 'Do not assert: serial <= n in jsxml.c';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

var pa = <state1 xmlns="http://aaa" id="D1"><tag1/></state1>
var ch = <state2 id="D2"><tag2/></state2>
pa.appendChild(ch);
pa.@msg = "Before assertion failure";
pa.toXMLString();

TEST(1, expect, actual);

END();
