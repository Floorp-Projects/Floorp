// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 374160;
var summary = 'Do not assert with <a><b c="1"></b><b c="2"></b></a>..@c[0]=3';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

<a><b c="1"></b><b c="2"></b></a>..@c[0] = 3;
TEST(1, expect, actual);

END();
