// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.5.4.2 - XMLList attribute()");

TEST(1, true, XMLList.prototype.hasOwnProperty("attribute"));

// Test with list of size 0
emps = new XMLList();
TEST(2, "xml", typeof(emps.attribute("id")));
TEST_XML(3, "", emps.attribute("id"));

// Test with list of size 1
emps += <employee id="0"><name>Jim</name><age>25</age></employee>;

TEST(4, "xml", typeof(emps.attribute("id")));
TEST_XML(5, 0, emps.attribute("id"));

// Test with list of size > 1
emps += <employee id="1"><name>Joe</name><age>20</age></employee>;
TEST(6, "xml", typeof(emps.attribute("id")));

correct = new XMLList();
correct += new XML("0");
correct += new XML("1");
TEST(7, correct, emps.attribute("id"));

// Test one that doesn't exist - should return empty XMLList
TEST(8, "xml", typeof(emps.attribute("foobar")));

// Test args of null and undefined
try {
  emps.attribute(null);
  SHOULD_THROW(9);
} catch (ex) {
  TEST(9, "TypeError", ex.name);
}

try {
  emps.attribute(undefined);
  SHOULD_THROW(10);
} catch (ex) {
  TEST(10, "TypeError", ex.name);
}

END();
