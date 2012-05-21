// |reftest| skip -- obsolete test
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 361451;
var summary = 'Do not crash with E4X, watch, import';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

var obj = <z><yyy/></z>;
obj.watch('x', print);
try { import obj.yyy; } catch(e) { }
obj = undefined;
gc();

TEST(1, expect, actual);

END();
