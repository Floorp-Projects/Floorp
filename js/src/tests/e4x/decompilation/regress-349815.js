/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 349815;
var summary = 'decompilation of parameterized e4x xmllist literal';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

var f = function (tag) { return <><{tag}></{tag}></>; }

expect = 'function (tag) {\n    return <><{tag}></{tag}></>;\n}';
actual = f + '';

compareSource(expect, actual, inSection(1) + summary);

END();
