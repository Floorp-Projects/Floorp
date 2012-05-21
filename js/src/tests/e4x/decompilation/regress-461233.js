/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = 'Decompilation of ({0: (4, <></>)})';
var BUGNUMBER = 461233;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

var f = (function() { return ({0: (4, <></>) }); });

expect = 'function() { return {0: (4, <></>) }; }';
actual = f + '';

compareSource(expect, actual, expect)

END();
