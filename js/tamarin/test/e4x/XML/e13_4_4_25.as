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

START("13.4.4.25 - XML nodeKind()");

//TEST(1, true, XML.prototype.hasOwnProperty("nodeKind"));

x1 =
<alpha attr1="value1">
    <bravo>one</bravo>
</alpha>;

TEST(2, "element", x1.bravo.nodeKind());
TEST(3, "attribute", x1.@attr1.nodeKind());

// Non-existant node type is text
x1 = new XML();
TEST(4, "text", x1.nodeKind());
//TEST(5, "text", XML.prototype.nodeKind());

var xmlDoc = "<company><employee id='1'><name1>John</name1> <city>California</city> </employee></company>";


AddTestCase( "MYXML = new XML(xmlDoc), MYXML.nodeKind()", 
	"element", 
	(MYXML = new XML(xmlDoc), MYXML.nodeKind()));

AddTestCase( "MYXML = new XML(null), MYXML.nodeKind()", 
	"text", 
	(MYXML = new XML(null), MYXML.nodeKind()));

AddTestCase( "MYXML = new XML(undefined), MYXML.nodeKind()", 
	"text", 
	(MYXML = new XML(undefined), MYXML.nodeKind()));

AddTestCase( "MYXML = new XML(), MYXML.nodeKind()", 
	"text", 
	(MYXML = new XML(), MYXML.nodeKind()));

AddTestCase( "MYXML = new XML(), MYXML.children()[0].nodeKind()", 
	"element", 
	(MYXML = new XML(xmlDoc), MYXML.children()[0].nodeKind()));

AddTestCase( "MYXML = new XML(), MYXML.children()[0].attributes()[0].nodeKind()", 
	"attribute", 
	(MYXML = new XML(xmlDoc), MYXML.children()[0].attributes()[0].nodeKind()));

AddTestCase( "MYXML = new XML(), MYXML.children()[0].name1.children()[0].nodeKind()", 
	"text", 
	(MYXML = new XML(xmlDoc), MYXML.children()[0].name1.children()[0].nodeKind()));

XML.ignoreComments = false
AddTestCase( "MYXML = new XML(\"<!-- this is a comment -->\"), MYXML.nodeKind()", 
	"element", 
	(MYXML = new XML("<XML><!-- this is a comment --></XML>"), MYXML.nodeKind()));

AddTestCase( "MYXML = new XML(\"<!-- this is a comment -->\"), MYXML.children()[0].nodeKind()", 
	"comment", 
	(MYXML = new XML("<XML><!-- this is a comment --></XML>"), MYXML.children()[0].nodeKind()));

XML.ignoreProcessingInstructions = false
AddTestCase( "MYXML = new XML(\"<XML><?foo this is a pi ?></XML>\"), MYXML.nodeKind()", 
	"element", 
	(MYXML = new XML("<XML><?foo-- this is a pi--?></XML>"), MYXML.nodeKind()));

AddTestCase( "MYXML = new XML(\"<XML><?foo this is a pi ?></XML>\"), MYXML.children()[0].nodeKind()", 
	"processing-instruction", 
	(MYXML = new XML("<XML><?foo this is a pi ?></XML>"), MYXML.children()[0].nodeKind()));


END();
