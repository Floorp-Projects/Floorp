/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 374163;
var summary =
  "Set E4X xml.function::__proto__ = null shouldn't affect " +
  "xml.[[DefaultValue]]";
var actual = 'unset';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

var a = <a/>;
a.function::__proto__ = null;
actual = "" + a;

TEST(1, expect, actual);
END();
