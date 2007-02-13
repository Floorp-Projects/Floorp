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

START("13.4.4.34 - XML setLocalName()");

//TEST(1, true, XML.prototype.hasOwnProperty("setLocalName"));

x1 =
<alpha>
    <bravo>one</bravo>
</alpha>;

correct =
<charlie>
    <bravo>one</bravo>
</charlie>;

x1.setLocalName("charlie");

TEST(2, correct, x1);

x1 =
<ns:alpha xmlns:ns="http://foobar/">
    <ns:bravo>one</ns:bravo>
</ns:alpha>;

correct =
<ns:charlie xmlns:ns="http://foobar/">
    <ns:bravo>one</ns:bravo>
</ns:charlie>;

x1.setLocalName("charlie");

TEST(3, correct, x1);

XML.prettyPrinting = false;
var xmlDoc = "<employee id='1'><firstname>John</firstname><lastname>Walton</lastname><age>25</age></employee>"

// propertyName as a string
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.setLocalName('newlocalname'),MYXML.localName()", 
	"newlocalname", 
	(MYXML = new XML(xmlDoc), MYXML.setLocalName('newlocalname'),MYXML.localName()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.setLocalName('newlocalname'),MYXML.toString()", 
	"<newlocalname id=\"1\"><firstname>John</firstname><lastname>Walton</lastname><age>25</age></newlocalname>", 
	(MYXML = new XML(xmlDoc), MYXML.setLocalName('newlocalname'),MYXML.toString()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.setLocalName(new QName('newlocalname')),MYXML.toString()", 
	"<newlocalname id=\"1\"><firstname>John</firstname><lastname>Walton</lastname><age>25</age></newlocalname>", 
	(MYXML = new XML(xmlDoc), MYXML.setLocalName(QName('newlocalname')),MYXML.toString()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.setLocalName(new QName('foo', 'newlocalname')),MYXML.toString()", 
	"<newlocalname id=\"1\"><firstname>John</firstname><lastname>Walton</lastname><age>25</age></newlocalname>", 
	(MYXML = new XML(xmlDoc), MYXML.setLocalName(QName('foo', 'newlocalname')),MYXML.toString()));

MYXML = new XML(xmlDoc); 

try {
	MYXML.setLocalName('@newlocalname');
	result = "no error";
} catch (e1) {
	result = typeError(e1.toString());
}

AddTestCase( "setLocalName('@newlocalname')", "TypeError: Error #1117", result);


try {
	MYXML.setLocalName('*');
	result = "no error";
} catch (e2) {
	result = typeError(e2.toString());
}

AddTestCase( "setLocalName('*')", "TypeError: Error #1117", result);

try {
	MYXML.setLocalName('x123=5');
	result = "no error";
} catch (e3) {
	result = typeError(e3.toString());
}

AddTestCase( "setLocalName('x123=5')", "TypeError: Error #1117", result);

try {
	MYXML.setLocalName('123');
	result = "no error";
} catch (e4) {
	result = typeError(e4.toString());
}

AddTestCase( "setLocalName('123')", "TypeError: Error #1117", result);

try {
	MYXML.setLocalName('!bam');
	result = "no error";
} catch (e5) {
	result = typeError(e5.toString());
}

AddTestCase( "setLocalName('!bam')", "TypeError: Error #1117", result);

END();
