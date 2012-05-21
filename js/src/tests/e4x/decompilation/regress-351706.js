/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 351706;
var summary = 'decompilation of E4X literals with parens';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

var f;

f = function() { return <{m}/>.(y) }
expect = 'function() { return (<{m}/>).(y); }';
actual = f + '';
compareSource(expect, actual, inSection(1) + summary);

f = function() { return (<{m}/>).(y) }
expect = 'function() { return (<{m}/>).(y); }';
actual = f + '';
compareSource(expect, actual, inSection(2) + summary);

END();
