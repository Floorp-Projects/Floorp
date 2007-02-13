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
 *   Rob Ginda rginda@netscape.com
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
START("11.1.1 - Attribute Identifiers");
x1 =
<alpha>
    <bravo attr1="value1" ns:attr1="value3" xmlns:ns="http://someuri">
        <charlie attr1="value2" ns:attr1="value4"/>
    </bravo>
</alpha>
    
TEST_XML(1, "value1", x1.bravo.@attr1);
TEST_XML(2, "value2", x1.bravo.charlie.@attr1);

correct = new XMLList();
correct += new XML("value1");
correct += new XML("value2");
TEST(3, correct, x1..@attr1);

n = new Namespace("http://someuri");
TEST_XML(4, "value3", x1.bravo.@n::attr1);
TEST_XML(5, "value4", x1.bravo.charlie.@n::attr1);

correct = new XMLList();
correct += new XML("value3");
correct += new XML("value4");
TEST(6, correct, x1..@n::attr1);

q = new QName("attr1");
q2 = new QName(q, "attr1");

TEST(7.3, "attr1", q.toString());
TEST(7.4, "attr1", q2.toString());

q = new QName(n, "attr1");
q2 = new QName(q, "attr1");
TEST(7, correct, x1..@[q]);
TEST(7.1, "http://someuri::attr1", q.toString());
TEST(7.2, "http://someuri::attr1", q2.toString());

correct = new XMLList();
correct += new XML("value1");
correct += new XML("value3");
correct += new XML("value2");
correct += new XML("value4");
TEST(8, correct, x1..@*::attr1);

TEST_XML(9, "value1", x1.bravo.@["attr1"]);
TEST_XML(10, "value3", x1.bravo.@n::["attr1"]);
TEST_XML(11, "value3", x1.bravo.@[q]);
TEST_XML(12, "value3", x1.bravo.@[q2]);


y = <ns:attr1 xmlns:ns="http://someuri"/>
q3 = y.name();

AddTestCase("q3 = y.name()", "http://someuri::attr1", q3.toString());
AddTestCase("x1.bravo.@[q3]", new XML("value3"), x1.bravo.@[q3]);


var xml1 = "<c><color c='1'>pink</color><color c='2'>purple</color><color c='3'>orange</color></c>";
var xml2 = "<animals><a a='dog'>Spot</a><a a='fish'>Marcel</a><a a='giraffe'>Virginia</a></animals>";
var xml3 = "<flowers><flower type='tulip' attr-with-hyphen='got it'><color>yellow</color></flower></flowers>";
var xml4 = "<ns1:a xmlns:ns1=\"http://yo-raps.tv\"><ns1:b attr=\"something\">rainbow</ns1:b></ns1:a>";

try {
	var xml5 = <x><myTag myAttrib="has an apostrophe' in it"/></x>;
	var res = "no exception";
	AddTestCase("Attribute with apostrophe in it", "has an apostrophe' in it", xml5.myTag.@myAttrib.toString());
} catch (e1) {
	var res = "exception";
} finally {
	// Needs to be fixed when bug 133471 is fixed
	AddTestCase("Attribute with apostrophe in it", "no exception", res);
}
	

var placeHolder = "c";

var ns1 = new Namespace('yo', 'http://yo-raps.tv');
var ns2 = new Namespace('mo', 'http://maureen.name');

AddTestCase("x1.node1[i].@attr", "1",
           ( x1 = new XML(xml1), x1.color[0].@c.toString()));

AddTestCase("x1.node1[i].@attr = \"new value\"", "5",
           ( x1 = new XML(xml1), x1.color[0].@c = "5", x1.color[0].@c.toString()));

AddTestCase("x1.node1[i].@[placeHolder]", "1",
           ( x1 = new XML(xml1), x1.color[0].@[placeHolder].toString()));

AddTestCase("x1.node1[i].@[placeHolder] = \"new value\"", "5",
           ( x1 = new XML(xml1), x1.color[0].@[placeHolder] = "5", x1.color[0].@[placeHolder].toString()));

AddTestCase("x1.node1[i].@attr", "giraffe",
           ( x1 = new XML(xml2), x1.a[2].@a.toString()));

AddTestCase("x1.node1[i].@attr = \"new value\"", "hippopotamus",
           ( x1 = new XML(xml2), x1.a[2].@a = "hippopotamus", x1.a[2].@a.toString()));

AddTestCase("x1.node1.@[attr-with-hyphen]", "got it",
           ( x1 = new XML(xml3), x1.flower.@["attr-with-hyphen"].toString()));

AddTestCase("x1.node1.@[attr-with-hyphen] = \"new value\"", "still got it",
           ( x1 = new XML(xml3), x1.flower.@["attr-with-hyphen"] = "still got it", x1.flower.@["attr-with-hyphen"].toString()));
           
AddTestCase("x1.namespace1::node1.@attr", "something",
           ( x1 = new XML(xml4), x1.ns1::b.@attr.toString()));

AddTestCase("x1.namespace1::node1.@attr = \"new value\"", "something else",
           ( x1 = new XML(xml4), x1.ns1::b.@attr = "something else", x1.ns1::b.@attr.toString()));
           

var ns = new Namespace("foo");
var y1 = <y xmlns:ns="foo" a="10" b="20" ns:c="30" ns:d="40"/>;
var an = 'a';

AddTestCase("y1.@a", "10", y1.@a.toString());

AddTestCase("y1.@[an]", "10", y1.@[an].toString());

AddTestCase("y1.@*", "10203040", y1.@*.toString());  // Rhino bug: doesn't include qualified attributes

AddTestCase("y1.@ns::*", 2, y1.@ns::*.length());

var z = <y xmlns:ns="foo" a="10" b="20" ns:c="30" ns:d="40"/>;
AddTestCase("y1.@b", "200", (z.@b = 200, z.@b.toString()));

AddTestCase("y1.@*", "103040", (delete y1.@b, y1.@*.toString()));

// Adding for bug 117159
var element:XML = new XML(<element function="foo"/>);
AddTestCase("Reserved keyword used as attribute name", "foo", element.@["function"].toString());

var xmlObj = new XML ();
xmlObj = XML ('<elem attr1="firstAttribute"></elem>');

try {
	e = xmlObj.(@nonExistentAttribute == "nonExistent");
	result = e;
} catch (e2) {
	result = referenceError(e2.toString());
}
AddTestCase("Access non-existent attribute", "ReferenceError: Error #1065", result);

END();
