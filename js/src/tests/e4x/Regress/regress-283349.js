/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "13.3.5.4 - [[GetNamespace]]";
var BUGNUMBER = 283349;
var actual = 'Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

var x = <x>text</x>;
var ns = new Namespace("http://foo.com/bar");
x.addNamespace(ns);
printStatus(x.toXMLString());

actual = 'No Crash';

TEST("13.3.5.4 - [[GetNamespace]]", expect, actual);

END();
