/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Igor Bukanov
 *   Ethan Hugg
 *   Milen Nankov
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

START("11.2.1 - Property Accessors");

order = 
<order id="123456" timestamp="Mon Mar 10 2003 16:03:25 GMT-0800 (PST)">
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

correct = 
<customer>
    <firstname>John</firstname>
    <lastname>Doe</lastname>
</customer>;

TEST(1, correct, order.customer);
TEST_XML(2, 123456, order.@id);

correct = 
<item>
    <description>Big Screen Television</description>
    <price>1299.99</price>
    <quantity>1</quantity>
</item>

TEST(3, correct, order.children()[1]);

correct = 
<customer>
    <firstname>John</firstname>
    <lastname>Doe</lastname>
</customer> +
<item>
    <description>Big Screen Television</description>
    <price>1299.99</price>
    <quantity>1</quantity>
</item>;


TEST(4, correct, order.*); 

correct = new XMLList();
correct += new XML("123456");
correct += new XML("Mon Mar 10 2003 16:03:25 GMT-0800 (PST)");
TEST(5, correct, order.@*);

order = <order>
        <customer>
            <firstname>John</firstname>
            <lastname>Doe</lastname>
        </customer>
        <item id="3456">
            <description>Big Screen Television</description>
            <price>1299.99</price>
            <quantity>1</quantity>
        </item>
        <item id="56789">
            <description>DVD Player</description>
            <price>399.99</price>
            <quantity>1</quantity>
        </item>
        </order>;

correct =
<description>Big Screen Television</description> +
<description>DVD Player</description>;

TEST(6, correct, order.item.description);

correct = new XMLList();
correct += new XML("3456");
correct += new XML("56789");
TEST(7, correct, order.item.@id);

correct =
<item id="56789">
    <description>DVD Player</description>
    <price>399.99</price>
    <quantity>1</quantity>
</item>

TEST(8, correct, order.item[1]);

correct = 
<description>Big Screen Television</description> +
<price>1299.99</price> +
<quantity>1</quantity> +
<description>DVD Player</description> +
<price>399.99</price> +
<quantity>1</quantity>;

TEST(9, correct, order.item.*);

correct=
<price>1299.99</price>;

TEST(10, correct, order.item.*[1]);

// get the first (and only) order [treating single element as a list]
order = <order>
        <customer>
            <firstname>John</firstname>
            <lastname>Doe</lastname>
        </customer>
        <item id="3456">
            <description>Big Screen Television</description>
            <price>1299.99</price>
            <quantity>1</quantity>
        </item>
        <item id="56789">
            <description>DVD Player</description>
            <price>399.99</price>
            <quantity>1</quantity>
        </item>
        </order>;


TEST(11, order, order[0]);

// Any other index should return undefined
TEST(12, undefined, order[1]);

var xml = "<order id = \"123456\"><c><f>John</f><l>Doe</l></c><i><desc>B</desc><p>1299.99</p><q>1</q></i><i><desc>A</desc><p>12.99</p><q>1</q></i></order>";

// XML object test cases

AddTestCase("XML.c.f:", "John", (o = new XML(xml), o.c.f.toString()));

AddTestCase("XML.c['f']:", "John", (o = new XML(xml), o.c['f'].toString()));

AddTestCase("XML.c.f[0]:", "John", (o = new XML(xml), o.c.f[0].toString()));

AddTestCase("XML.i[1].p:", "12.99", (o = new XML(xml), o.i[1].p.toString()));

AddTestCase("XML.i[1]['p]'", "12.99", (o = new XML(xml), o.i[1]['p'].toString()));


// High ASCII test
var xmlHighASCII = "<f><fname>Sören Lehmenkühler</fname></f>";

AddTestCase("High ASCII node value:", "Sören Lehmenkühler", (o = new XML(xmlHighASCII), o.fname.toString()));


// XMLList object test cases

AddTestCase("XMLList.c.f:", "John", (ol = new XMLList(xml), ol.c.f.toString()));

AddTestCase("XMLList.c[\"f\"]:", "John", (ol = new XMLList(xml), ol.c["f"].toString()));

AddTestCase("XMLList.c.f[0]:", "John", (o = new XMLList(xml), o.c.f[0].toString()));

AddTestCase("XMLList.c.f[0] = \"Peter\":", "Peter", (o = new XMLList(xml), o.c.f[0] = "Peter", o.c.f[0].toString()));

AddTestCase("XMLList.i[1].p:", "12.99", (ol = new XMLList(xml), ol.i[1].p.toString()));

AddTestCase("XMLList.i[1][\"p\"]:", "12.99", (ol = new XMLList(xml), ol.i[1]["p"].toString()));

AddTestCase("XMLList[1] = <a>b</a>", "b", (ol = new XMLList(), ol[1] = <a>b</a>, ol.toString())); 

AddTestCase("XMLList[1] = <a>b</a>; XMLList[0] = <c>d</c>", "d", (ol = new XMLList(), ol[1] = <a>b</a>, ol[0] = <c>d</c>, ol.toString())); 

AddTestCase("XMLList[0] = <a>b</a>; XMLList[1] = <c>d</c>", new XMLList("<a>b</a><c>d</c>"), (ol = new XMLList(), ol[0] = <a>b</a>, ol[1] = <c>d</c>, ol)); 


var x1 = new XML("<x><fname>a</fname><fname>b</fname><fname>c</fname></x>");
var y1 = x1.fname;

AddTestCase("x1.f == x1.f[0] + x1.f[1] + x1.f[2]", x1.fname[0] + x1.fname[1] + x1.fname[2],
x1.fname);

// comparing XML and XMLList equivalents

AddTestCase("XML[0].fname[1] == XMLList[1]:", true, (y1 = x1.fname, (x1[0].fname[1] == y1[1])));

AddTestCase("XML[0].fname[0] == XMLList.fname[0]:", true, (y1 = new XMLList(x1), (x1[0].fname[0] == y1.fname[0])));

 
var hyphenatedXML = new XML("<a><b-c a='1'>blue</b-c><b-c a='2'>orange</b-c><b-c a='3'>yellow</b-c></a>");

AddTestCase("hyphenatedXML.[\"b-c\"]:", "orange", (hyphenatedXML["b-c"][1].toString()));

AddTestCase("hyphenatedXML.[\"b-c\"][1] = \"new color\":", "pink", (hyphenatedXML["b-c"][1] = "pink", hyphenatedXML["b-c"][1].toString()));

xL = <x a="aatr" b="batr">y</x>;

AddTestCase("x['*']", "y", xL['*'].toString());
AddTestCase("x['@*']", "aatrbatr", xL['@*'].toString());
AddTestCase("x['@a']", "aatr", xL['@a'].toString());

xL = <x xmlns:ns1="foo" xmlns:ns2="bar" ns1:prop='prop1' ns2:prop='prop2' prop='prop3'>some text</x>; 

AddTestCase("x1.@prop", "prop3", xL.@prop.toString());

function setNS1() {
	use namespace foo;
	AddTestCase("use namespace foo; x1.@prop", "prop1prop3", xL.@prop.toString());
}

function setNS2() {
	namespace foo2 = "bar"; 
	use namespace foo2;
	AddTestCase("use namespace foo2; x1.@prop", "prop2prop3", xL.@prop.toString());
}

function setNS3() {
	use namespace foo;
	namespace foo2 = "bar"; 
	use namespace foo2;
	AddTestCase("use namespace foo2; x1.@prop", "prop1prop2prop3", xL.@prop.toString());
}

namespace foo = "foo"; 

setNS1();

setNS2();

setNS3();

END();
