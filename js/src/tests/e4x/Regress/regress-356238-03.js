// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 356238;
var summary = 'bug 356238';
var actual = 'No Error';
var expect = 'No Error';

printBugNumber(BUGNUMBER);
START(summary);

var xml = <xml><child><a/></child></xml>;
var child = xml.child[0];

try {
  child.insertChildBefore(null, xml.child);
  actual = "insertChildBefore succeded when it should throw an exception";
} catch (e) {
}

var list = child.*;
var text = uneval(list[1]);
if (!/(undefined|\(void 0\))/.test(text))
  throw "child got unexpected second element: "+text;

TEST(1, expect, actual);
END();
