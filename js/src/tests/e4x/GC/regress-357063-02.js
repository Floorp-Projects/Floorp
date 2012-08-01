// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 357063;
var summary = 'GC hazard in XMLEquality';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

var xml1 = new XML("<xml>text<a>B</a></xml>");
var xml2 = new XML("<xml>text<a>C</a></xml>");

XML.prototype.function::toString = function() {
  if (xml2) {
    delete xml2.*;
    xml2 = null;
    gc();
  }
  return "text";
}

print('xml1: ' + xml1);
print('xml2: ' + xml2);

if (xml1 == xml2)
  throw "unexpected result";

TEST(1, expect, actual);

END();
