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

START("13.4.4.21 - XML localName()");

//TEST(1, true, XML.prototype.hasOwnProperty("localName"));

x1 = new XML("<alpha><bravo>one</bravo><charlie><bravo>two</bravo></charlie></alpha>");
var y1;
y1 = x1.localName();
TEST(2, "string", typeof(y1));
TEST(3, "alpha", y1);

y1 = x1.bravo.localName();
x1.bravo.setNamespace("http://someuri");
TEST(4, "bravo", y1);

x1 = 
<foo:alpha xmlns:foo="http://foo/">
    <foo:bravo name="bar" foo:value="one">one</foo:bravo>
</foo:alpha>;

ns = new Namespace("http://foo/");
y1 = x1.ns::bravo.localName();
TEST(5, "string", typeof(y1));
TEST(6, "bravo", y1);

y1 = x1.ns::bravo.@name.localName();
TEST(7, "name", y1);

y1 = x1.ns::bravo.@ns::value.localName();
TEST(8, "value", y1);

var xmlDoc = "<company xmlns:printer='http://colors.com/printer/'><printer:employee id='1'><name>John</name> <city>California</city> </printer:employee></company>";

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.localName()", 
	"company", 
	(MYXML = new XML(xmlDoc), MYXML.localName()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.localName() instanceof QName", 
	false, 
	(MYXML = new XML(xmlDoc), MYXML.localName() instanceof QName));

// !!@ fails in Rhino??
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.localName() instanceof String", 
	true, 
	(MYXML = new XML(xmlDoc), MYXML.localName() instanceof String));

AddTestCase( "MYXML = new XML(xmlDoc), typeof(MYXML.localName())", 
	"string", 
	(MYXML = new XML(xmlDoc), typeof(MYXML.localName())));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.children()[0].localName()", 
	"employee", 
	(MYXML = new XML(xmlDoc), MYXML.children()[0].localName()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.children()[0].localName() instanceof QName", 
	false, 
	(MYXML = new XML(xmlDoc), MYXML.children()[0].localName() instanceof QName));

// !!@ fails in Rhino??
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.children()[0].localName() instanceof String", 
	true, 
	(MYXML = new XML(xmlDoc), MYXML.children()[0].localName() instanceof String));

AddTestCase( "MYXML = new XML(xmlDoc), typeof(MYXML.children()[0].localName())", 
	"string", 
	(MYXML = new XML(xmlDoc), typeof(MYXML.children()[0].localName())));

END();
