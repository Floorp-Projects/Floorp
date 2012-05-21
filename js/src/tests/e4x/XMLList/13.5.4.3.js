/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.5.4.3 - XMLList attributes()");

TEST(1, true, XMLList.prototype.hasOwnProperty("attributes"));

// Test with XMLList of size 0
x = new XMLList()
TEST(2, "xml", typeof(x.attributes()));
TEST_XML(3, "", x.attributes());

// Test with XMLList of size 1
x += <alpha attr1="value1" attr2="value2">one</alpha>;

TEST(4, "xml", typeof(x.attributes()));
correct = new XMLList();
correct += new XML("value1");
correct += new XML("value2");
TEST(5, correct, x.attributes());

// Test with XMLList of size > 1
x += <bravo attr3="value3" attr4="value4">two</bravo>;

TEST(6, "xml", typeof(x.attributes()));
correct = new XMLList();
correct += new XML("value1");
correct += new XML("value2");
correct += new XML("value3");
correct += new XML("value4");
TEST(7, correct, x.attributes());

END();
