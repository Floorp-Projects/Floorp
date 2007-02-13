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

START("13.4.4.2 - XML addNamespace()");

//TEST(1, true, XML.prototype.hasOwnProperty("addNamespace"));
//TEST(2, true, XML.prototype.hasOwnProperty("children"));

e =
<employees>
    <employee id="0"><name>Jim</name><age>25</age></employee>
    <employee id="1"><name>Joe</name><age>20</age></employee>
</employees>;

n = "http://foobar/";
e.addNamespace(n);

n = new Namespace();
e.addNamespace(n);

n = new Namespace("http://foobar/");
e.addNamespace(n);

x1 = <a/>;
n = new Namespace("ns", "uri");
x1.addNamespace(n);
TEST(2, "<a xmlns:ns=\"uri\"/>", x1.toXMLString());

var n1 = new Namespace('pfx', 'http://w3.org');
var n2 = new Namespace('http://us.gov');
var n3 = new Namespace('boo', 'http://us.gov');
var n4 = new Namespace('boo', 'http://hk.com');
var xml = "<a><b s='1'><c>55</c><d>bird</d></b><b s='2'><c>5</c><d>dinosaur</d></b></a>";
var xmlwithns = "<a xmlns:pfx=\"http://w3.org\"><a><b s='1'><c>55</c><d>bird</d></b><b s='2'><c>5</c><d>dinosaur</d></b></a>";

AddTestCase( "addNamespace with prefix:", "http://w3.org", 
           (  x1 = new XML(xml), x1.addNamespace(n1), myGetNamespace(x1,'pfx').toString()));

AddTestCase( "addNamespace without prefix:", undefined,
           (  x1 = new XML(xml), x1.addNamespace(n2), myGetNamespace(x1, 'blah')));

expectedStr = "ArgumentError: Error #1063: Argument count mismatch on XML/addNamespace(). Expected 1, got 0";
expected = "Error #1063";
actual = "No error";

try {
	x1.addNamespace();
} catch(e1) {
	actual = grabError(e1, e1.toString());
}

AddTestCase( "addNamespace(): Error. Needs 1 argument", expected, actual);

AddTestCase( "Does namespace w/o prefix change XML object:", true,
           (  x1 = new XML(xml), y1 = new XML(xml), x1.addNamespace(n1), (x1 == y1)));

AddTestCase( "Does namespace w/ prefix change XML object:", true,
           (  x1 = new XML(xml), y1 = new XML(xml), x1.addNamespace(n2), (x1 == y1)));

AddTestCase( "Adding two different namespaces:", 'http://w3.org', 
	   (  x1 = new XML(xml), x1.addNamespace(n1), x1.addNamespace(n3), myGetNamespace(x1, 'pfx').toString()));

AddTestCase( "Adding two different namespaces:", 'http://us.gov',
           (  x1 = new XML(xml), x1.addNamespace(n1), x1.addNamespace(n3), myGetNamespace(x1, 'boo').toString()));

AddTestCase( "Adding namespace with pre-existing prefix:", 'http://hk.com',
           (  x1 = new XML(xml), x1.addNamespace(n3), x1.addNamespace(n4), myGetNamespace(x1, 'boo').toString()));


AddTestCase( "Adding namespace to something other than top node:", 'http://hk.com',
           (  x1 = new XML(xml), x1.b[0].d.addNamespace(n4), myGetNamespace(x1.b[0].d, 'boo').toString()));


AddTestCase( "Adding namespace to XMLList element:", 'http://hk.com',
           (  x1 = new XMLList(xml), x1.b[1].addNamespace(n4), myGetNamespace(x1.b[1], 'boo').toString()));
           


// namespaces with prefixes are preserved

x2 = <ns2:x xmlns:ns2="foo"><b>text</b></ns2:x>;
x2s = x2.toString();
correct = '<ns2:x xmlns:ns2="foo">\n  <b>text</b>\n</ns2:x>';
AddTestCase("Original XML", x2s, correct);

// Adding a namespace to a node will clear a conflicting prefix
var ns = new Namespace ("ns2", "newuri");
x2.addNamespace (ns);
x2s = x2.toString();
 
correct = '<x xmlns:ns2="newuri" xmlns="foo">\n  <b>text</b>\n</x>';

AddTestCase("Adding namespace that previously existed with a different prefix", correct, 
	   x2s);


correct = <newprefix:x xmlns:newprefix="foo"><b>text</b></newprefix:x>;

AddTestCase("Adding totally new namespace", correct, 
(ns3 = new Namespace ("newprefix", "foo"), x2.addNamespace (ns3),x2));

END();
