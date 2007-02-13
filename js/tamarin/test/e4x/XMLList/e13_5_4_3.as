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

START("13.5.4.3 - XMLList attributes()");

//TEST(1, true, XMLList.prototype.hasOwnProperty("attributes"));

// Test with XMLList of size 0
x1 = new XMLList()
TEST(2, "xml", typeof(x1.attributes()));
TEST_XML(3, "", x1.attributes());

// Test with XMLList of size 1
x1 += <alpha attr1="value1" attr2="value2">one</alpha>;

TEST(4, "xml", typeof(x1.attributes()));
correct = new XMLList();
correct += new XML("value1");
correct += new XML("value2");
TEST(5, correct, x1.attributes());

// Test with XMLList of size > 1
x1 += <bravo attr3="value3" attr4="value4">two</bravo>;

TEST(6, "xml", typeof(x1.attributes()));
correct = new XMLList();
correct += new XML("value1");
correct += new XML("value2");
correct += new XML("value3");
correct += new XML("value4");
TEST(7, correct, x1.attributes());

var xmlDoc = "<TEAM foo = 'bar' two='second'>Giants</TEAM><CITY two = 'third'>Giants</CITY>";

AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.attributes() instanceof XMLList", true, 
             (MYXML = new XMLList(xmlDoc), MYXML.attributes() instanceof XMLList ));
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.attributes() instanceof XML", false, 
             (MYXML = new XMLList(xmlDoc), MYXML.attributes() instanceof XML ));
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.attributes().length()", 3, 
             (MYXML = new XMLList(xmlDoc), MYXML.attributes().length() ));
XML.prettyPrinting = false;
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.attributes().toString()", "barsecondthird", 
             (MYXML = new XMLList(xmlDoc), MYXML.attributes().toString() ));
XML.prettyPrinting = true;
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.attributes().toString()", "barsecondthird", 
             (MYXML = new XMLList(xmlDoc), MYXML.attributes().toString() ));
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.attributes()[0].nodeKind()", "attribute", 
             (MYXML = new XMLList(xmlDoc), MYXML.attributes()[0].nodeKind() ));
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.attributes()[1].nodeKind()", "attribute", 
             (MYXML = new XMLList(xmlDoc), MYXML.attributes()[1].nodeKind() ));
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.attributes()[2].nodeKind()", "attribute", 
             (MYXML = new XMLList(xmlDoc), MYXML.attributes()[2].nodeKind() ));

AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.attributes().toXMLString()", "bar" + NL() + "second" + NL() + "third", 
             (MYXML = new XMLList(xmlDoc), MYXML.attributes().toXMLString() ));

END();
