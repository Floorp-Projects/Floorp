/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 352649;
var summary = 'decompilation of e4x literal after |if| block';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

var f = function() { if(g) p; (<x/>.i) }
expect = 'function() { if(g) {p;} <x/>.i; }';
actual = f + '';
compareSource(expect, actual, inSection(1) + summary);

END();
