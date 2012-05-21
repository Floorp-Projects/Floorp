/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 350226;
var summary = 'decompilation of <x/>.@[*]';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

var f = function () { <x/>[@[*]]; }
expect = 'function () {\n    <x/>[@[*]];\n}';
actual = f + '';

compareSource(expect, actual, inSection(1) + summary);

END();
