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

START("13.4.4.38 - XML toString()");

//TEST(1, true, XML.prototype.hasOwnProperty("toString"));

XML.prettyPrinting = false;

x1 = 
<alpha>
    <bravo>one</bravo>
    <charlie>
        <bravo>two</bravo>
    </charlie>
</alpha>;

TEST(2, "one", x1.bravo.toString());
TEST(3, "<bravo>one</bravo>" + NL() + "<bravo>two</bravo>", x1..bravo.toString());

x1 = 
<alpha>
    <bravo>one</bravo>
    <charlie/>
</alpha>;

TEST(4, "", x1.charlie.toString());

x1 = 
<alpha>
    <bravo>one</bravo>
    <charlie>
        <bravo>two</bravo>
    </charlie>
</alpha>;

TEST(5, "<charlie><bravo>two</bravo></charlie>", x1.charlie.toString());

x1 = 
<alpha>
    <bravo>one</bravo>
    <charlie>
        two
        <bravo/>
    </charlie>
</alpha>;

TEST(5, "<charlie>two<bravo/></charlie>", x1.charlie.toString());

x1 = 
<alpha>
    <bravo></bravo>
    <bravo/>
</alpha>;

TEST(6, "<bravo/>" + NL() + "<bravo/>", x1.bravo.toString());

x1 = 
<alpha>
    <bravo>one<charlie/></bravo>
    <bravo>two<charlie/></bravo>
</alpha>;

TEST(7, "<bravo>one<charlie/></bravo>" + NL() + "<bravo>two<charlie/></bravo>", x1.bravo.toString());

var xmlDoc = "<employees><employee id='1'><firstname>John</firstname><lastname>Walton</lastname><age>25</age></employee><employee id='2'><firstname>Sue</firstname><lastname>Day</lastname><age>32</age></employee></employees>"


XML.prettyPrinting = false;
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.employee[0].toString()", 
	"<employee id=\"1\"><firstname>John</firstname><lastname>Walton</lastname><age>25</age></employee>", 
    (MYXML = new XML(xmlDoc), MYXML.employee[0].toString()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.employee[1].toString()", 
	"<employee id=\"2\"><firstname>Sue</firstname><lastname>Day</lastname><age>32</age></employee>", 
    (MYXML = new XML(xmlDoc), MYXML.employee[1].toString()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.employee[0].firstname.toString()", 
	"John", 
    (MYXML = new XML(xmlDoc), MYXML.employee[0].firstname.toString()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.employee[1].firstname.toString()", 
	"Sue", 
    (MYXML = new XML(xmlDoc), MYXML.employee[1].firstname.toString()));

XML.prettyPrinting = true;
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.employee[0].toString()", 
	"<employee id=\"1\">" + NL() + "  <firstname>John</firstname>" + NL() + "  <lastname>Walton</lastname>" + NL() + "  <age>25</age>" + NL() + "</employee>", 
    (MYXML = new XML(xmlDoc), MYXML.employee[0].toString()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.employee[1].toString()", 
	"<employee id=\"2\">" + NL() + "  <firstname>Sue</firstname>" + NL() + "  <lastname>Day</lastname>" + NL() + "  <age>32</age>" + NL() + "</employee>", 
    (MYXML = new XML(xmlDoc), MYXML.employee[1].toString()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.employee[0].firstname.toString()", 
	"John", 
    (MYXML = new XML(xmlDoc), MYXML.employee[0].firstname.toString()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.employee[1].firstname.toString()", 
	"Sue", 
    (MYXML = new XML(xmlDoc), MYXML.employee[1].firstname.toString()));

xmlDoc = new XML("<XML>foo</XML>");

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.toString()", 
	"foo", 
    (MYXML = new XML(xmlDoc), MYXML.toString()));
    
END();
