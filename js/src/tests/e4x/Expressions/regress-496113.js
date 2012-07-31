// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// See https://developer.mozilla.org/en/Core_JavaScript_1.5_Guide/Processing_XML_with_E4X#section_7


var summary = 'simple filter';
var BUGNUMBER = 496113;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

var people = <people>
 <person>
  <name>Joe</name>
 </person>
</people>;

expect = <person><name>Joe</name></person>.toXMLString();

print(actual = people.person.(name == "Joe").toXMLString());

TEST(1, expect, actual);

END();
