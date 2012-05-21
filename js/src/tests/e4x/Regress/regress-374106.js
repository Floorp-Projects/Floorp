/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 374106;
var summary = 'e4x XMLList.contains execution halts with complex match';
var actual = 'No Error';
var expect = 'No Error';

printBugNumber(BUGNUMBER);
START(summary);

<><x><y/></x></>.contains(<x><y/></x>); 3;

TEST(1, expect, actual);
END();
