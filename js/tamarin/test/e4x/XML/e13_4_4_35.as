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

START("13.4.4.35 - setName");

//TEST(1, true, XML.prototype.hasOwnProperty("setName"));

x1 =
<alpha>
    <bravo>one</bravo>
</alpha>;

correct =
<charlie>
    <bravo>one</bravo>
</charlie>;

x1.setName("charlie");

TEST(2, correct, x1);

x1 =
<ns:alpha xmlns:ns="http://foobar/">
    <ns:bravo>one</ns:bravo>
</ns:alpha>;

correct =
<charlie xmlns:ns="http://foobar/">
    <ns:bravo>one</ns:bravo>
</charlie>;

x1.setName("charlie");

TEST(3, correct, x1);

x1 =
<ns:alpha xmlns:ns="http://foobar/">
    <ns:bravo>one</ns:bravo>
</ns:alpha>;

correct =
<ns:charlie xmlns:ns="http://foobar/">
    <ns:bravo>one</ns:bravo>
</ns:charlie>;

x1.setName(new QName("http://foobar/", "charlie"));

TEST(4, correct, x1);

XML.prettyPrinting = false;
var xmlDoc = "<company><employee id='1'><name>John</name> <city>California</city> </employee></company>";

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.setName('employees'),MYXML.name().toString()", 
	"employees", 
	(MYXML = new XML(xmlDoc),MYXML.setName('employees'), MYXML.name().toString()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.setName(new QName('employees')),MYXML.name().toString()", 
	"employees", 
	(MYXML = new XML(xmlDoc),MYXML.setName(new QName('employees')), MYXML.name().toString()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.setName(new QName('ns', 'employees')),MYXML.name().toString()", 
	"ns::employees", 
	(MYXML = new XML(xmlDoc),MYXML.setName(new QName('ns', 'employees')), MYXML.name().toString()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.setName('employees'),MYXML.toString()", 
	"<employees><employee id=\"1\"><name>John</name><city>California</city></employee></employees>", 
	(MYXML = new XML(xmlDoc),MYXML.setName('employees'), MYXML.toString()));

// Calling setName() on an attribute
AddTestCase("MYXML = new XML(xmlDoc), MYXML.employee.@id.setName('num')", "num", (MYXML = new XML(xmlDoc), MYXML.employee.@id.setName("num"), MYXML.employee.@num.name().toString()));

var TYPEERROR = "TypeError: Error #";
function typeError( str ){
	return str.slice(0,TYPEERROR.length+4);
}
MYXML = new XML(xmlDoc); 
MYXML.employee.@id.setName("num"); 

try {
	MYXML.employee.@id.name();
	result = "no error";
} catch (e1) {
	result = typeError(e1.toString());
}

AddTestCase("MYXML = new XML(xmlDoc), MYXML.employee.@id.setName(\"num\"),MYXML.employee.@id.name())", "TypeError: Error #1086", result);
x1 =
<foo:alpha xmlns:foo="http://foo/" xmlns:bar="http://bar/">
    <foo:bravo attr="1">one</foo:bravo>
</foo:alpha>;

ns = new Namespace("foo", "http://foo/");
correct = <foo:alpha xmlns:foo="http://foo/" xmlns:bar="http://bar/">
    <foo:bravo foo="1">one</foo:bravo>
</foo:alpha>;

AddTestCase("Calling setName() on an attribute with same name as namespace", correct, (x1.ns::bravo.@attr.setName("foo"), x1));

// throws Rhino exception - bad name
MYXML = new XML(xmlDoc);
try {
	MYXML.setName('@employees');
	result = " no error";
} catch (e2) {
	result = typeError(e2.toString());
}
AddTestCase("MYXML.setName('@employees')", "TypeError: Error #1117", result);

try {
	MYXML.setName('!hi');
	result = " no error";
} catch (e3) {
	result = typeError(e3.toString());
}
AddTestCase("MYXML.setName('!hi')", "TypeError: Error #1117", result);

try {
	MYXML.setName('1+1=5');
	result = " no error";
} catch (e4) {
	result = typeError(e4.toString());
}
AddTestCase("MYXML.setName('1+1=5')", "TypeError: Error #1117", result);

try {
	MYXML.setName('555');
	result = " no error";
} catch (e5) {
	result = typeError(e5.toString());
}
AddTestCase("MYXML.setName('555')", "TypeError: Error #1117", result);


try {
	MYXML.setName('1love');
	result = " no error";
} catch (e6) {
	result = typeError(e6.toString());
}
AddTestCase("MYXML.setName('1love')", "TypeError: Error #1117", result);

try {
	MYXML.setName('*');
	result = " no error";
} catch (e7) {
	result = typeError(e7.toString());
}
AddTestCase("MYXML.setName('*')", "TypeError: Error #1117", result);



END();
