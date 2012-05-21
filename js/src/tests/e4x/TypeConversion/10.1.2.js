/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("10.1.2 - XMLList.toString");

var n = 0;
var expect;
var actual;
var xmllist;
var xml;

// Example from ECMA 357 10.1.1

var order =
<order>
    <customer>
        <firstname>John</firstname>
        <lastname>Doe</lastname>
    </customer>
    <item>
        <description>Big Screen Television</description>
        <price>1299.99</price>
        <quantity>1</quantity>
    </item>
</order>;

// Semantics

printStatus("test empty.toString()");

xmllist = new XMLList();
expect = '';
actual = xmllist.toString();
TEST(++n, expect, actual);

printStatus("test [attribute].toString()");

xmllist = new XMLList();
xml = <foo bar="baz"/>;
var attr = xml.@bar;
xmllist[0] = attr;
expect = "baz";
actual = xmllist.toString();
TEST(++n, expect, actual);

printStatus("test [text].toString()");

xmllist = new XMLList();
xml = new XML("this is text");
xmllist[0] = xml;
expect = "this is text";
actual = xmllist.toString();
TEST(++n, expect, actual);

printStatus("test [simpleContent].toString()");

xmllist = new XMLList();
xml = <foo>bar</foo>;
xmllist[0] = xml;
expect = "bar";
actual = xmllist.toString();
TEST(++n, expect, actual);

var truefalse = [true, false];

for (var ic = 0; ic < truefalse.length; ic++)
{
    for (var ip = 0; ip < truefalse.length; ip++)
    {
        XML.ignoreComments = truefalse[ic];
        XML.ignoreProcessingInstructions = truefalse[ip];

        xmllist = new XMLList();
        xml = <foo><!-- comment1 --><?pi1 ?>ba<!-- comment2 -->r<?pi2 ?></foo>;
        xmllist[0] = xml;
        expect = "bar";
        actual = xmllist.toString();
        TEST(++n, expect, actual);
    }
}

printStatus("test nonSimpleContent.toString() == " +
            "nonSimpleContent.toXMLString()");

var indents = [0, 4];

for (var pp = 0; pp < truefalse.length; pp++)
{
    XML.prettyPrinting = truefalse[pp];
    for (var pi = 0; pi < indents.length; pi++)
    {
        XML.prettyIndent = indents[pi];
  
        xmllist = new XMLList(order);
        expect = order.toXMLString();
        actual = order.toString();
        TEST(++n, expect, actual);

        xmllist = order..*;
        expect = order.toXMLString();
        actual = order.toString();
        TEST(++n, expect, actual);

    }
}

END();
