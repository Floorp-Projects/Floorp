// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = '13.3 QName Objects - Do not assert: op == JSOP_ADD';
var BUGNUMBER = 373595;
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

print('This testcase does not reproduce the original issue');

"" + (function () { x.@foo < 1;});

TEST(1, expect, actual);

END();
