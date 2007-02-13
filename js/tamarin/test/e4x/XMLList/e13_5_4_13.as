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

START("13.5.4.13 - XMLList hasSimpleContent()");

//TEST(1, true, XMLList.prototype.hasOwnProperty("hasSimpleContent"));

// One element should be same as XML case
x1 = 
<>
<alpha attr1="value1">
    <bravo>one</bravo>
    <charlie>
        two
        three
        <echo>four</echo>
    </charlie>
    <delta />
    <foxtrot attr2="value2">five</foxtrot>
    <golf attr3="value3" />
    <hotel>
        six
        seven
    </hotel>
    <india><juliet /></india>
</alpha>;
</>;

TEST(2, false, x1.hasSimpleContent());
TEST(3, true, x1.bravo.hasSimpleContent());
TEST(4, false, x1.charlie.hasSimpleContent());
TEST(5, true, x1.delta.hasSimpleContent());
TEST(6, true, x1.foxtrot.hasSimpleContent());
TEST(7, true, x1.golf.hasSimpleContent());
TEST(8, true, x1.hotel.hasSimpleContent());
TEST(9, true, x1.@attr1.hasSimpleContent());
TEST(10, true, x1.bravo.child(0).hasSimpleContent());
TEST(11, false, x1.india.hasSimpleContent());

// More than one element is complex if one or more things in the list are elements.
x1 = 
<>
<alpha>one</alpha>
<bravo>two</bravo>
</>;

TEST(12, false, x1.hasSimpleContent());

x1 =
<root>
    one
    <alpha>one</alpha>
</root>;

TEST(13, false, x1.*.hasSimpleContent());

x1 =
<root attr1="value1" attr2="value2">
    one
</root>;

TEST(14, true, x1.@*.hasSimpleContent());

var xmlDoc = "<employee><firstname>John</firstname><lastname>Walton</lastname><age>25</age></employee>"

AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.hasSimpleContent()", true, 
(MYXML = new XMLList(xmlDoc), MYXML.hasComplexContent()));

xmlDoc = "<firstname>John</firstname>"  
AddTestCase ("MYXML = new XML(xmlDoc), MYXML.children().length()", 1,   
	  (MYXML = new XML(xmlDoc), MYXML.children().length()));
AddTestCase ("MYXML = new XML(xmlDoc), MYXML.children()[0].nodeKind()", "text",   
	  (MYXML = new XML(xmlDoc), MYXML.children()[0].nodeKind()));
AddTestCase ("MYXML = new XML(xmlDoc), MYXML.children()[0].nodeKind()", "text",   
	  (MYXML = new XML(xmlDoc), MYXML.children()[0].nodeKind()));
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.children()[0].hasSimpleContent()", true, 
	  (MYXML = new XML(xmlDoc), MYXML.children()[0].hasSimpleContent()));

XML.ignoreComments = false;
xmlDoc = "<XML><!-- firstname --></XML>"
AddTestCase ("MYXML = new XML(xmlDoc), MYXML.children().length()", 1,   
	  (MYXML = new XML(xmlDoc), MYXML.children().length()));
AddTestCase ("MYXML = new XML(xmlDoc), MYXML.children()[0].nodeKind()", "comment",   
	  (MYXML = new XML(xmlDoc), MYXML.children()[0].nodeKind()));
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.children()[0].hasSimpleContent()", false, 
	  (MYXML = new XML(xmlDoc), MYXML.children()[0].hasSimpleContent()));

XML.ignoreProcessingInstructions = false;
xmlDoc = "<XML><?xml-stylesheet href=\"classic.xsl\" type=\"text/xml\"?></XML>"  
AddTestCase ("MYXML = new XML(xmlDoc), MYXML.children().length()", 1,   
	  (MYXML = new XML(xmlDoc), MYXML.children().length()));
AddTestCase ("MYXML = new XML(xmlDoc), MYXML.children()[0].nodeKind()", "processing-instruction",   
	  (MYXML = new XML(xmlDoc), MYXML.children()[0].nodeKind()));
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.children()[0].hasSimpleContent()", false, 
	  (MYXML = new XML(xmlDoc), MYXML.children()[0].hasSimpleContent()));

xmlDoc = "<XML a='5' b='c'>foo</XML>"  
AddTestCase ("MYXML = new XML(xmlDoc), MYXML.children().length()", 1,   
	  (MYXML = new XML(xmlDoc), MYXML.children().length()));
AddTestCase ("MYXML = new XML(xmlDoc), MYXML.children()[0].nodeKind()", "text",   
	  (MYXML = new XML(xmlDoc), MYXML.children()[0].nodeKind()));
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.children()[0].hasSimpleContent()", true, 
	  (MYXML = new XML(xmlDoc), MYXML.children()[0].hasSimpleContent()));
	  
END();
