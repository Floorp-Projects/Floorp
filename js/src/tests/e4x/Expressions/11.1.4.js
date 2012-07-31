// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("11.1.4 - XML Initializer");

person = <person><name>John</name><age>25</age></person>;
TEST(1, <person><name>John</name><age>25</age></person>, person);   

e = <employees>
    <employee id = "1"><name>Joe</name><age>20</age></employee>
    <employee id = "2"><name>Sue</name><age>30</age></employee>
    </employees>;

TEST_XML(2, 1, e.employee[0].@id);

correct = <name>Sue</name>;
TEST(3, correct, e.employee[1].name);

names = new Array();
names[0] = "Alpha";
names[1] = "Bravo";
names[2] = "Charlie";
names[3] = "Delta";
names[4] = "Echo";
names[5] = "Golf";
names[6] = "Hotel";
names[7] = "India";
names[8] = "Juliet";
names[9] = "Kilo";

ages = new Array();
ages[0] = "20";
ages[1] = "21";
ages[2] = "22";
ages[3] = "23";
ages[4] = "24";
ages[5] = "25";
ages[6] = "26";
ages[7] = "27";
ages[8] = "28";
ages[9] = "29";

for (i = 0; i < 10; i++)
{
    e.*[i] = <employee id={i}>
           <name>{names[i].toUpperCase()}</name>
           <age>{ages[i]}</age>
           </employee>;

    correct = new XML("<employee id=\"" + i + "\"><name>" + names[i].toUpperCase() + "</name><age>" + ages[i] + "</age></employee>");
    TEST(4 + i, correct, e.*[i]);
}

tagName = "name";
attributeName = "id";
attributeValue = 5;
content = "Fred";

x = <{tagName} {attributeName}={attributeValue}>{content}</{tagName}>;
TEST(14, "<name id=\"5\">Fred</name>", x.toXMLString());

// Test {} on XML and XMLList types
x =
<rectangle>
    <length>30</length>
    <width>50</width>
</rectangle>;

correct =
<rectangle>
    <width>50</width>
    <length>30</length>
</rectangle>;

x = <rectangle>{x.width}{x.length}</rectangle>;

TEST(15, correct, x);

var content = "<foo name=\"value\">bar</foo>";
x = <x><a>{content}</a></x>;
correct = <x/>;
correct.a = content;
TEST(16, correct, x);

x = <x a={content}/>;
correct = <x/>;
correct.@a = content;
TEST(17, correct, x);

a = 5;
b = 3;
c = "x";
x = <{c} a={a + " < " + b + " is " + (a < b)}>{a + " < " + b + " is " + (a < b)}</{c}>;
TEST(18, "<x a=\"5 &lt; 3 is false\">5 &lt; 3 is false</x>", x.toXMLString());

x = <{c} a={a + " > " + b + " is " + (a > b)}>{a + " > " + b + " is " + (a > b)}</{c}>;
TEST(19, "<x a=\"5 > 3 is true\">5 &gt; 3 is true</x>", x.toXMLString());

END();
