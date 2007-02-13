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

START("13.4.4.39 - XML toXMLString");

//TEST(1, true, XML.prototype.hasOwnProperty("toXMLString"));

XML.prettyPrinting = false;

x1 = 
<alpha>
    <bravo>one</bravo>
    <charlie>
        <bravo>two</bravo>
    </charlie>
</alpha>;

TEST(2, "<bravo>one</bravo>", x1.bravo.toXMLString());
TEST(3, "<bravo>one</bravo>" + NL() + "<bravo>two</bravo>", x1..bravo.toXMLString());

x1 = 
<alpha>
    <bravo>one</bravo>
    <charlie/>
</alpha>;

TEST(4, "<charlie/>", x1.charlie.toXMLString());

x1 = 
<alpha>
    <bravo>one</bravo>
    <charlie>
        <bravo>two</bravo>
    </charlie>
</alpha>;

TEST(5, "<charlie><bravo>two</bravo></charlie>", x1.charlie.toXMLString());

x1 = 
<alpha>
    <bravo>one</bravo>
    <charlie>
        two
        <bravo/>
    </charlie>
</alpha>;

TEST(6, "<charlie>two<bravo/></charlie>", x1.charlie.toXMLString());

x1 = 
<alpha>
    <bravo></bravo>
    <bravo/>
</alpha>;

TEST(7, "<bravo/>" + NL() + "<bravo/>", x1.bravo.toXMLString());

x1 = 
<alpha>
    <bravo>one<charlie/></bravo>
    <bravo>two<charlie/></bravo>
</alpha>;

TEST(8, "<bravo>one<charlie/></bravo>" + NL() + "<bravo>two<charlie/></bravo>", x1.bravo.toXMLString());
   
XML.prettyPrinting = true;

x1 = 
<alpha>
    <bravo>one</bravo>
    <charlie>two</charlie>
</alpha>;

copy = x1.bravo.copy();

TEST(9, "<bravo>one</bravo>", copy.toXMLString());

x1 = 
<alpha>
    <bravo>one</bravo>
    <charlie>
        <bravo>two</bravo>
    </charlie>
</alpha>;

TEST(10, "String contains value one from bravo", "String contains value " + x1.bravo + " from bravo");

var xmlDoc = "<employees><employee id=\"1\"><firstname>John</firstname><lastname>Walton</lastname><age>25</age></employee><employee id=\"2\"><firstname>Sue</firstname><lastname>Day</lastname><age>32</age></employee></employees>"


// propertyName as a string
XML.prettyPrinting = false;
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.employee[0].toXMLString()", 
	"<employee id=\"1\"><firstname>John</firstname><lastname>Walton</lastname><age>25</age></employee>", 
    (MYXML = new XML(xmlDoc), MYXML.employee[0].toXMLString()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.employee[1].toXMLString()", 
	"<employee id=\"2\"><firstname>Sue</firstname><lastname>Day</lastname><age>32</age></employee>", 
    (MYXML = new XML(xmlDoc), MYXML.employee[1].toXMLString()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.employee[0].firstname.toXMLString()", 
	"<firstname>John</firstname>", 
    (MYXML = new XML(xmlDoc), MYXML.employee[0].firstname.toXMLString()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.employee[1].firstname.toXMLString()", 
	"<firstname>Sue</firstname>", 
    (MYXML = new XML(xmlDoc), MYXML.employee[1].firstname.toXMLString()));

// test case containing xml namespaces
var xmlDoc2 = "<ns2:table2 foo=\"5\" xmlns=\"http://www.w3schools.com/furniture\" ns2:bar=\"last\" xmlns:ns2=\"http://www.macromedia.com/home\"><ns2:name>African Coffee Table</ns2:name><width>80</width><length>120</length></ns2:table2>";
AddTestCase( "MYXML = new XML(xmlDoc2), MYXML.toXMLString()", 
	"<ns2:table2 foo=\"5\" ns2:bar=\"last\" xmlns=\"http://www.w3schools.com/furniture\" xmlns:ns2=\"http://www.macromedia.com/home\"><ns2:name>African Coffee Table</ns2:name><width>80</width><length>120</length></ns2:table2>", 
    (MYXML = new XML(xmlDoc2), MYXML.toXMLString()));


XML.prettyPrinting = true;
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.employee[0].toXMLString()", 
	"<employee id=\"1\">" + NL() + "  <firstname>John</firstname>" + NL() + "  <lastname>Walton</lastname>" + NL() + "  <age>25</age>" + NL() + "</employee>", 
    (MYXML = new XML(xmlDoc), MYXML.employee[0].toXMLString()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.employee[1].toXMLString()", 
	"<employee id=\"2\">" + NL() + "  <firstname>Sue</firstname>" + NL() + "  <lastname>Day</lastname>" + NL() + "  <age>32</age>" + NL() + "</employee>", 
    (MYXML = new XML(xmlDoc), MYXML.employee[1].toXMLString()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.employee[0].firstname.toXMLString()", 
	"<firstname>John</firstname>", 
    (MYXML = new XML(xmlDoc), MYXML.employee[0].firstname.toXMLString()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.employee[1].firstname.toXMLString()", 
	"<firstname>Sue</firstname>", 
    (MYXML = new XML(xmlDoc), MYXML.employee[1].firstname.toXMLString()));

xmlDoc = "<XML>foo</XML>";
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.toXMLString()", 
	"<XML>foo</XML>", 
    (MYXML = new XML(xmlDoc), MYXML.toXMLString()));
    
END();
