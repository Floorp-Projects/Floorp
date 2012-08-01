// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "13.4.4.3 - XML.appendChild should copy child";

var BUGNUMBER = 312692;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

var ul = <ul/>;
var li = <li/>;

li.setChildren("First");
ul.appendChild(li);

li.setChildren("Second");
ul.appendChild(li);

XML.ignoreWhitespace = true;
XML.prettyPrinting = true;

expect = (
  <ul>
    <li>Second</li>
    <li>Second</li>
  </ul>
    ).toXMLString().replace(/</g, '&lt;');

actual = ul.toXMLString().replace(/</g, '&lt;');

TEST(1, expect, actual);

END();
