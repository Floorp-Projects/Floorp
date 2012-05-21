// |reftest| skip -- obsolete test
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 373678;
var summary = 'Missing quotes around string in decompilation, ' +
    'with for..in and do..while';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

var f = function() { do {for(a.b in []) { } } while("c\\d"); };
expect = 'function() { do {for(a.b in []) { } } while("c\\\\d"); }';
actual = f + '';
compareSource(expect, actual, inSection(1) + summary);

END();
