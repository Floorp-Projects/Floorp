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

START("13.5.4.16 - XMLList parent()");

//TEST(1, true, XMLList.prototype.hasOwnProperty("parent"));
    
// Empty should return undefined
x1 = new XML();
TEST(2, undefined, x1.parent());

// If all XMLList items have same parent, then return that parent.
x1 =
<alpha>
    <bravo>one</bravo>
    <charlie>two</charlie>
    <bravo>three<charlie>four</charlie></bravo>
</alpha>;

y1 = new XMLList();
y1 += x1.bravo;
y1 += x1.charlie;

TEST(3, x1, y1.parent());

// If items have different parents then return undefined
y1 = new XMLList();
y1 += x1.bravo;
y1 += x1.bravo.charlie;

TEST(4, undefined, y1.parent());

y1 = x1..charlie;

TEST(5, undefined, y1.parent());

var xDoc = "<doc><employee id='1'><a>b</a></employee><employee id='2'><a>c</a></employee></doc>";
var MYXML = new XMLList(xDoc);

AddTestCase( "MYXML = new XMLList(xDoc), MYXML.employee[0].parent()", MYXML.name(), 
             (MYXML.employee[0].parent().name()));

xDoc = "<employee id='1'><firstname>John</firstname><lastname>Walton</lastname><age>25</age></employee><company><name>Macromedia</name></company>";

AddTestCase( "MYXML = new XMLList(xDoc), MYXML.parent()", undefined, 
             (MYXML = new XMLList(xDoc), MYXML.parent()));

var x1 = new XMLList("<XML>text node<foo>bar</foo></XML>");
var a = new XMLList("text node");
var b = new XMLList("<foo>bar</foo>");

AddTestCase ("MYXML = new('<XML></XML>'), MYXML.parent()", undefined, x1.parent());
AddTestCase ("MYXML = new('text node'), MYXML.parent()", undefined, a.parent());
AddTestCase ("MYXML = new('<foo>bar</foo>'), MYXML.parent()", undefined, b.parent());

//x.appendChild (a);
//x.appendChild (b);

// Appendchild duplicates a and b so their parent should still be null
AddTestCase ("a - orphan node, a.parent()", undefined, a.parent());
AddTestCase ("b - orphan node, b.parent()", undefined, b.parent());

AddTestCase ("x1.children()[0].parent()", x1[0], x1.children()[0].parent());
AddTestCase ("x1.children()[1].parent()", x1[0], x1.children()[0].parent());
AddTestCase ("x1.children()[0].parent() === x1[0]", true, (x1.children()[0].parent() === x1[0]));
AddTestCase ("x1.children()[1].parent() === x1[0]", true, (x1.children()[0].parent() === x1[0]));

xDoc = "<company><employee id='1'><name1>John</name1> <city>California</city> </employee><employee id='2'><name1>Mary</name1> <city>Texas</city> </employee></company>";

AddTestCase( "x1 = new XMLList(xDoc), x1.employee[0].parent() == x1[0]", true, (x1 = new XMLList(xDoc), x1.employee[0].parent() == x1[0]));
AddTestCase( "x1 = new XMLList(xDoc), x1.employee.name1[0].parent() == x1.employee[0]", true, (x1 = new XMLList(xDoc), x1.employee.name1[0].parent() == x1.employee[0]));
AddTestCase( "x1 = new XMLList(xDoc), x1.employee[0].attribute('id').parent() === x1.employee[0]", true, (x1 = new XMLList(xDoc), x1.employee[0].attribute('id').parent() === x1.employee[0]));

AddTestCase( "x1 = new XMLList(xDoc), x1.employee[1].parent() == x1[0]", true, (x1 = new XMLList(xDoc), x1.employee[1].parent() == x1[0]));
AddTestCase( "x1 = new XMLList(xDoc), x1.employee.name1[1].parent() == x1.employee[0]", true, (x1 = new XMLList(xDoc), x1.employee.name1[1].parent() == x1.employee[1]));
AddTestCase( "x1 = new XMLList(xDoc), x1.employee[1].attribute('id').parent() === x1.employee[1]", true, (x1 = new XMLList(xDoc), x1.employee[1].attribute('id').parent() === x1.employee[1]));

XML.ignoreComments = false;
XML.ignoreProcessingInstructions = false;
xDoc = "<simple><!-- comment --><?x-stylesheet href=\"classic.xsl\" type=\"text/x\"?></simple>";

// Tests comments and PI nodes
AddTestCase( "x1 = new XMLList(xDoc), x1.children()[0].parent() == x1[0]", true, (x1 = new XMLList(xDoc), x1.children()[0].parent() == x1[0]));
AddTestCase( "x1 = new XMLList(xDoc), x1.children()[1].parent() == x1[0]", true, (x1 = new XMLList(xDoc), x1.children()[1].parent() == x1[0]));

END();
