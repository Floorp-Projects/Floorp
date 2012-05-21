/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("10.1.1 - XML.toString");

var n = 0;
var expect;
var actual;
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

var name = order.customer.firstname + " " + order.customer.lastname;

TEST(++n, "John Doe", name);

var total = order.item.price * order.item.quantity;

TEST(++n, 1299.99, total);

// Semantics

printStatus("test empty.toString()");

xml = new XML();
expect = '';
actual = xml.toString();
TEST(++n, expect, actual);

printStatus("test attribute.toString()");

xml = <foo bar="baz"/>;
var attr = xml.@bar;
expect = "baz";
actual = attr.toString();
TEST(++n, expect, actual);

printStatus("test text.toString()");

xml = new XML("this is text");
expect = "this is text";
actual = xml.toString();
TEST(++n, expect, actual);

printStatus("test simpleContent.toString()");

xml = <foo>bar</foo>;
expect = "bar";
actual = xml.toString();
TEST(++n, expect, actual);

var truefalse = [true, false];

for (var ic = 0; ic < truefalse.length; ic++)
{
    for (var ip = 0; ip < truefalse.length; ip++)
    {
        XML.ignoreComments = truefalse[ic];
        XML.ignoreProcessingInstructions = truefalse[ip];

        xml = <foo><!-- comment1 --><?pi1 ?>ba<!-- comment2 -->r<?pi2 ?></foo>;
        expect = "bar";
        actual = xml.toString();
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
  
        expect = order.toXMLString();
        actual = order.toString();
        TEST(++n, expect, actual);
    }
}

END();

