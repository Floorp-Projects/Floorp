/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = 'Do not assert: kid2->parent == xml || !kid2->parent';
var BUGNUMBER = 369032;
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

var y = <y/>;
var b = y.b;
b.a = 3;
var x = <x/>.appendChild(b);
y.b = 5;

expect = '<y>\n  <b>5</b>\n</y>';
actual = y.toXMLString();
TEST(1, expect, actual);

expect = '<x>\n  <b>\n    <a>3</a>\n  </b>\n</x>';
actual = x.toXMLString();
TEST(2, expect, actual);

END();
