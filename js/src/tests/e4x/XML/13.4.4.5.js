// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.4.5 - XML attributes()");

TEST(1, true, XML.prototype.hasOwnProperty("attributes"));

x =
<alpha attr1="value1" attr2="value2" attr3="value3">
    <bravo>one</bravo>
</alpha>;

TEST(2, "xml", typeof(x.attributes()));

// Iterate through the attributes of an XML value
x =
<alpha attr1="value1" attr2="value2" attr3="value3">
    <bravo>one</bravo>
</alpha>;

correct = new Array("value1", "value2", "value3");
i = 0;

for each (var a in x.attributes())
{
    TEST_XML(i + 3, correct[i], a);
    i++;
}

END();
