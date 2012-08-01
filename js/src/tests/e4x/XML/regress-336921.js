// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = '13.4.4.3 - XML.prototype.appendChild creates undesired <br/>';
var BUGNUMBER = 336921;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

// test here

var array = ["a","b","c"];
var myDiv = <div/>;
for each (a in array) {
    myDiv.appendChild(a);
    myDiv.appendChild(<br/>);
}

actual = myDiv.toXMLString();
expect = '<div>\n  a\n  <br/>\n  b\n  <br/>\n  c\n  <br/>\n</div>';

TEST(1, expect, actual);

END();
