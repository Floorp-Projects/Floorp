// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.28 - processingInsructions()");

TEST(1, true, XML.prototype.hasOwnProperty("processingInstructions"));

XML.ignoreProcessingInstructions = false;

// test generic PI
x = <alpha><?xyz abc="123" michael="wierd"?><?another name="value" ?><bravo>one</bravo></alpha>;

correct = <><?xyz abc="123" michael="wierd"?><?another name="value" ?></>;

TEST(2, correct, x.processingInstructions());
TEST(3, correct, x.processingInstructions("*"));

correct = "<?xyz abc=\"123\" michael=\"wierd\"?>";

TEST_XML(4, correct, x.processingInstructions("xyz"));

// test XML-decl
// Un-comment these tests when we can read in doc starting with PI.
//x = new XML("<?xml version=\"1.0\" ?><alpha><bravo>one</bravo></alpha>");

//correct = new XMLList(<?xml version="1.0" encoding="utf-8"?>);

//test(5, correct, x.processingInstructions());
//test(6, correct, x.processingInstructions("*"));
//test(7, correct, x.processingInstructions("xml"));

END();
