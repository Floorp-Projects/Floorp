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

START("13.4.4.9 - XML comments()");

//TEST(1, true, XML.prototype.hasOwnProperty("comments"));

XML.ignoreComments = false;

x1 = new XML("<alpha><!-- comment one --><bravo><!-- comment two -->some text</bravo></alpha>");    

TEST_XML(2, "<!-- comment one -->", x1.comments());
TEST_XML(3, "<!-- comment two -->", x1..*.comments());

x2 = new XML("<alpha><!-- comment one --><!-- comment 1.5 --><bravo><!-- comment two -->some text<charlie><!-- comment three --></charlie></bravo></alpha>");    

TEST(4, "<!-- comment one -->\n<!-- comment 1.5 -->", x2.comments().toXMLString());
TEST(5, "<!-- comment two -->\n<!-- comment three -->", x2..*.comments().toXMLString());

XML.ignoreComments = true;
var xmlDoc = "<company><!-- This is Comment --><employee id='1'><!-- This is another Comment --><name>John</name> <city>California</city> </employee><!-- me too --></company>";

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.comments().toString()", "", (MYXML = new XML(xmlDoc), MYXML.comments().toString()));
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.comments() instanceof XMLList", true, (MYXML = new XML(xmlDoc), MYXML.comments() instanceof XMLList));
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.comments() instanceof XML", false, (MYXML = new XML(xmlDoc), MYXML.comments() instanceof XML));
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.comments().length()", 0, (MYXML = new XML(xmlDoc), MYXML.comments().length() ));

XML.prettyPrinting = true;
XML.ignoreComments = false;
AddTestCase( "XML.ignoreComments = false, MYXML = new XML(xmlDoc), MYXML.comments().toString()", 
	"<!-- This is Comment -->\n<!-- me too -->", 
	(XML.ignoreComments = false, MYXML = new XML(xmlDoc), MYXML.comments().toXMLString()));
AddTestCase( "XML.ignoreComments = false, MYXML = new XML(xmlDoc), MYXML.comments() instanceof XMLList", true, (MYXML = new XML(xmlDoc), MYXML.comments() instanceof XMLList));
AddTestCase( "XML.ignoreComments = false, MYXML = new XML(xmlDoc), MYXML.comments() instanceof XML", false, (MYXML = new XML(xmlDoc), MYXML.comments() instanceof XML));
AddTestCase( "XML.ignoreComments = false, MYXML = new XML(xmlDoc), MYXML.comments().length()", 2, (MYXML = new XML(xmlDoc), MYXML.comments().length() ));

AddTestCase( "XML.ignoreComments = false, MYXML = new XML(xmlDoc), XML.ignoreComments = true, MYXML.comments().toString()", 
	"<!-- This is Comment -->\n<!-- me too -->", 
	(XML.ignoreComments = false, MYXML = new XML(xmlDoc), XML.ignoreComments = true, MYXML.comments().toXMLString()));

END();
