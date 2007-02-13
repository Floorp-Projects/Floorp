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

START("13.4.4.19 - insertChildBefore()");

//TEST(1, true, XML.prototype.hasOwnProperty("insertChildBefore"));
    
x1 =
<alpha>
    <bravo>one</bravo>
    <charlie>two</charlie>
</alpha>;

correct =
<alpha>
    <delta>three</delta>
    <bravo>one</bravo>
    <charlie>two</charlie>
</alpha>;

x1.insertChildBefore(x1.bravo[0], <delta>three</delta>);

TEST(2, correct, x1);

x1 =
<alpha>
    <bravo>one</bravo>
    <charlie>two</charlie>
</alpha>;

correct =
<alpha>
    <bravo>one</bravo>
    <charlie>two</charlie>
    <delta>three</delta>
</alpha>;
x2 = new XML();

x2 = x1.insertChildBefore(null, <delta>three</delta>);

TEST(3, correct, x1);

TEST(4, correct, x2);

// to simplify string matching
XML.prettyPrinting = false;

var xmlDoc = "<company></company>";
var child1 = new XML("<employee id='1'><name>John</name></employee>");
var child2 = new XML("<employee id='2'><name>Sue</name></employee>");
var child3 = new XML("<employee id='3'><name>Bob</name></employee>");

var allChildren = new XMLList("<employee id='1'><name>John</name></employee><employee id='2'><name>Sue</name></employee><employee id='3'><name>Bob</name></employee>");
var twoChildren = new XMLList("<employee id='1'><name>John</name></employee><employee id='2'><name>Sue</name></employee>");

var wholeString = "<company><employee id='1'><name>John</name></employee><employee id='2'><name>Sue</name></employee><employee id='3'><name>Bob</name></employee></company>";

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.insertChildBefore(null, child1), MYXML.toString()", 
	"<company><employee id=\"1\"><name>John</name></employee></company>", 
	(MYXML = new XML(xmlDoc), MYXML.insertChildBefore(null, child1), MYXML.toString()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.insertChildBefore(null, child1), MYXML.children()[0].parent() == MYXML", 
	true, 
	(MYXML = new XML(xmlDoc), MYXML.insertChildBefore(null, child1), MYXML.children()[0].parent() == MYXML));

// The child is equal to child1 (but not the same object)
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.insertChildBefore(null, child1), MYXML.children()[0] == child1", 
	true, 
	(MYXML = new XML(xmlDoc), MYXML.insertChildBefore(null, child1), MYXML.children()[0] == child1));

// The child is a duplicate of child1
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.insertChildBefore(null, child1), MYXML.children()[0] === child1", 
	true, 
	(MYXML = new XML(xmlDoc), MYXML.insertChildBefore(null, child1), MYXML.children()[0] === child1));

var MYXML = new XML(xmlDoc);
MYXML.insertChildBefore(null, child1);

// !!@ this crashes Rhino's implementation
AddTestCase( "MYXML.insertChildBefore(child1, child2), MYXML.toString()", 
	"<company><employee id=\"2\"><name>Sue</name></employee><employee id=\"1\"><name>John</name></employee></company>", 
	(MYXML.insertChildBefore(child1, child2), MYXML.toString()));


var MYXML = new XML(xmlDoc);
MYXML.insertChildBefore(null, child1);

AddTestCase( "MYXML.insertChildBefore(MYXML.children()[0], child2), MYXML.toString()", 
	"<company><employee id=\"2\"><name>Sue</name></employee><employee id=\"1\"><name>John</name></employee></company>", 
	(MYXML.insertChildBefore(MYXML.children()[0], child2), MYXML.toString()));

MYXML = new XML(xmlDoc);
MYXML.insertChildBefore(null, child2);
MYXML.insertChildBefore(MYXML.children()[0], child1);

// !!@ this crashes Rhino's implementation
AddTestCase( "MYXML.insertChildBefore(child2, child3), MYXML.toString()", 
	"<company><employee id=\"1\"><name>John</name></employee><employee id=\"3\"><name>Bob</name></employee><employee id=\"2\"><name>Sue</name></employee></company>", 
	(MYXML.insertChildBefore(child2, child3), MYXML.toString()));

MYXML = new XML(xmlDoc);
MYXML.insertChildBefore(null, child2);
MYXML.insertChildBefore(MYXML.children()[0], child1);

AddTestCase( "MYXML.insertChildBefore(MYXML.children()[1], child3), MYXML.toString()", 
	"<company><employee id=\"1\"><name>John</name></employee><employee id=\"3\"><name>Bob</name></employee><employee id=\"2\"><name>Sue</name></employee></company>", 
	(MYXML.insertChildBefore(MYXML.children()[1], child3), MYXML.toString()));
	
MYXML = new XML(xmlDoc);

AddTestCase("MYXML.insertChildBefore(null, XMLList), MYXML.toString()", 
             new XML(wholeString).toString(),
             (MYXML.insertChildBefore(null, allChildren), MYXML.toString()));
             
MYXML = new XML(xmlDoc);
MYXML.insertChildBefore(null, child3);

AddTestCase("MYXML.insertChildBefore(child3, XMLList), MYXML.toString()", 
             new XML(wholeString).toString(),
             (MYXML.insertChildBefore(MYXML.children()[0], twoChildren), MYXML.toString()));
             
MYXML = new XML(xmlDoc);
MYXML.insertChildBefore(null, child1);

AddTestCase("MYXML.insertChildBefore(child1, \"string\"), MYXML.toString()",
	     new XML("<company>string<employee id='1'><name>John</name></employee></company>").toString(),
	     (MYXML.insertChildBefore(MYXML.children()[0], "string"), MYXML.toString()));
             
var a = <a><b><c/></b></a>;

try {
	a.b.c.insertChildBefore (null, a);
	result = a;
} catch (e1) {
	result = typeError(e1.toString());
}
AddTestCase("a = <a><b><c/></b></a>, a.b.c.insertChildBefore(null,a)", "TypeError: Error #1118", result);




END();
