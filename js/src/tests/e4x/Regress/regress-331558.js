/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 331558;
var summary = 'Decompiler: Missing = in default xml namespace statement';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

actual = (function () { default xml namespace = 'abc' }).toString();
expect = 'function () {\n    default xml namespace = "abc";\n}';

TEST(1, expect, actual);

END();
