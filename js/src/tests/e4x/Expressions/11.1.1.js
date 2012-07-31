// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("11.1.1 - Attribute Identifiers");

x =
<alpha>
    <bravo attr1="value1" ns:attr1="value3" xmlns:ns="http://someuri">
        <charlie attr1="value2" ns:attr1="value4"/>
    </bravo>
</alpha>
   
TEST_XML(1, "value1", x.bravo.@attr1);
TEST_XML(2, "value2", x.bravo.charlie.@attr1);

correct = new XMLList();
correct += new XML("value1");
correct += new XML("value2");
TEST(3, correct, x..@attr1);

n = new Namespace("http://someuri");
TEST_XML(4, "value3", x.bravo.@n::attr1);
TEST_XML(5, "value4", x.bravo.charlie.@n::attr1);

correct = new XMLList();
correct += new XML("value3");
correct += new XML("value4");
TEST(6, correct, x..@n::attr1);

q = new QName(n, "attr1");
TEST(7, correct, x..@[q]);

correct = new XMLList();
correct += new XML("value1");
correct += new XML("value3");
correct += new XML("value2");
correct += new XML("value4");
TEST(8, correct, x..@*::attr1);

TEST_XML(9, "value1", x.bravo.@["attr1"]);
TEST_XML(10, "value3", x.bravo.@n::["attr1"]);
TEST_XML(11, "value3", x.bravo.@[q]);

END();
