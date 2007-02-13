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

START("13.4.4.28 - processingInsructions()");

//TEST(1, true, XML.prototype.hasOwnProperty("processingInstructions"));

XML.ignoreProcessingInstructions = false;

// test generic PI
var x1 = new XML("<alpha><?xyz abc=\"123\" michael=\"weird\"?><?another name=\"value\" ?><bravo>one</bravo></alpha>");

var correct = new XMLList("<?xyz abc=\"123\" michael=\"weird\"?><?another name=\"value\" ?>");

TEST(2, correct, x1.processingInstructions());
TEST(3, correct, x1.processingInstructions("*"));

correct = "<?xyz abc=\"123\" michael=\"weird\"?>";

TEST_XML(4, correct, x1.processingInstructions("xyz"));

// test XML-decl 
// Un-comment these tests when we can read in doc starting with PI.
x1 = new XML("<?xml version=\"1.0\" ?><alpha><bravo>one</bravo></alpha>");

correct = new XMLList("<?xml version=\"1.0\" encoding=\"utf-8\"?>");

test(5, correct, x1.processingInstructions());
test(6, correct, x1.processingInstructions("*"));
test(7, correct, x1.processingInstructions("xml"));

// extra whitespace is on purpose for <?foo              bar> at the end of this string
var xmlDoc = "<?xml version='1.0'?><xml><?xml-stylesheet href='mystyle.xsl'?><employee id='1'><firstname>John</firstname><lastname>Walton</lastname><age>25</age></employee> <employee id='2'><firstname>Sue</firstname><lastname>Day</lastname><age>32</age><?child-xml bar?></employee><?foo              bar?></xml>"

// propertyName as a string
XML.ignoreProcessingInstructions = true;

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.processingInstructions().toString()", "", 
             (MYXML = new XML(xmlDoc), MYXML.processingInstructions().toString()));

XML.ignoreProcessingInstructions = false;

AddTestCase( "ignorePI = false, MYXML = new XML(xmlDoc), MYXML.processingInstructions().toString()", 
			 "<?xml-stylesheet href='mystyle.xsl'?>\n<?foo bar?>", 
             (MYXML = new XML(xmlDoc), MYXML.processingInstructions().toXMLString()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.processingInstructions('*')", 
	"<?xml-stylesheet href='mystyle.xsl'?>\n<?foo bar?>", 
	(MYXML = new XML(xmlDoc), MYXML.processingInstructions("*").toXMLString()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.processingInstructions('xml-stylesheet')", 
	"<?xml-stylesheet href='mystyle.xsl'?>", 
	(MYXML = new XML(xmlDoc), MYXML.processingInstructions("xml-stylesheet").toString()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.processingInstructions(new QName('xml-stylesheet'))", 
	"<?xml-stylesheet href='mystyle.xsl'?>", 
	(MYXML = new XML(xmlDoc), MYXML.processingInstructions(new QName("xml-stylesheet")).toString()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.processingInstructions(new QName('foo'))", 
	"<?foo bar?>", 
	(MYXML = new XML(xmlDoc), MYXML.processingInstructions(new QName("foo")).toString()));

// Attribute name does not match
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.processingInstructions('@xml-stylesheet')", 
	"", 
	(MYXML = new XML(xmlDoc), MYXML.processingInstructions("@xml-stylesheet").toString()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.processingInstructions('xml-foo')", 
	"", 
	(MYXML = new XML(xmlDoc), MYXML.processingInstructions("xml-foo").toString()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.processingInstructions('child-xml')", 
	"", 
	(MYXML = new XML(xmlDoc), MYXML.processingInstructions("child-xml").toString()));

END();
