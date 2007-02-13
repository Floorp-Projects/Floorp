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

START("13.4.4.36 - setNamespace");

//TEST(1, true, XML.prototype.hasOwnProperty("setNamespace"));

x1 =
<foo:alpha xmlns:foo="http://foo/" xmlns:bar="http://bar/">
    <foo:bravo>one</foo:bravo>
</foo:alpha>;

correct =
<bar:alpha xmlns:foo="http://foo/" xmlns:bar="http://bar/">
    <foo:bravo>one</foo:bravo>
</bar:alpha>;

x1.setNamespace("http://bar/");   

TEST(2, correct, x1);

XML.prettyPrinting = false;
var xmlDoc = "<xsl:stylesheet xmlns:xsl=\"http://www.w3.org/TR/xsl\"><xsl:template match=\"/\"><html>body</html></xsl:template></xsl:stylesheet>"

// !!@ Rhino comes with with the "ns" prefix for this namespace.  I have no idea how that happens.
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.setNamespace('http://xyx.org/xml'),MYXML.toString()", 
	"<stylesheet xmlns:xsl=\"http://www.w3.org/TR/xsl\" xmlns=\"http://xyx.org/xml\"><xsl:template match=\"/\"><html>body</html></xsl:template></stylesheet>", 
	(MYXML = new XML(xmlDoc), MYXML.setNamespace('http://xyx.org/xml'), MYXML.toString()));

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.setNamespace('http://xyx.org/xml'),MYXML.getNamespace()", 
	"http://xyx.org/xml", 
	(MYXML = new XML(xmlDoc), MYXML.setNamespace('http://xyx.org/xml'), myGetNamespace(MYXML).toString()));


xmlDoc = "<a><b><c>d</c></b></a>";
MYXML = new XML(xmlDoc);
MYXML.setNamespace('http://foo');
MYXML.b.c.setNamespace('http://bar');

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.setNamespace('http://zxz.org/xml'),MYXML.toString()", 
	"<a xmlns=\"http://foo\"><b><aaa:c xmlns:aaa=\"http://bar\">d</aaa:c></b></a>", 
	(MYXML.toString()));
	
var n1 = new Namespace('zzz', 'http://www.zzz.com');
var n2 = new Namespace('zzz', 'http://www.zzz.org');
var n3 = new Namespace('abc', 'http://www.zzz.com');
var n4 = new Namespace('def', 'http://www.zzz.com');

xmlDoc = "<a><b>c</b></a>";

AddTestCase("Adding two namespaces by uri", 
	"<a xmlns=\"http://www.zzz.com\"><aaa:b xmlns:aaa=\"http://www.zzz.org\">c</aaa:b></a>", 
	(MYXML = new XML(xmlDoc), MYXML.setNamespace('http://www.zzz.com'), MYXML.b.setNamespace('http://www.zzz.org'), MYXML.toString()));

AddTestCase("Adding two namespaces with prefix 'zzz'", 
	"<zzz:a xmlns:zzz=\"http://www.zzz.com\"><zzz:b xmlns:zzz=\"http://www.zzz.org\">c</zzz:b></zzz:a>", 
	(MYXML = new XML(xmlDoc), MYXML.setNamespace(n1), MYXML.b.setNamespace(n2), MYXML.toString()));
	
AddTestCase("Adding two namespaces with prefix 'zzz'", 
	"<abc:a xmlns:abc=\"http://www.zzz.com\"><abc:b xmlns:def=\"http://www.zzz.com\">c</abc:b></abc:a>", 
	(MYXML = new XML(xmlDoc), MYXML.setNamespace(n3), MYXML.b.setNamespace(n4), MYXML.toString()));
	

ns = new Namespace("moo", "http://moo/");
ns2 = new Namespace("zoo", "http://zoo/");
ns3 = new Namespace("noo", "http://mootar");
ns4 = new Namespace("moo", "http://bar/");
ns5 = new Namespace("poo", "http://moo/");
x1 = <moo:alpha xmlns:moo="http://moo/" xmlns:tar="http://tar/">
    <moo:bravo attr="1">one</moo:bravo>
</moo:alpha>;

correct = <moo:alpha xmlns:moo="http://moo/" xmlns:tar="http://tar/">
    <moo:bravo zoo:attr="1" xmlns:zoo="http://zoo/">one</moo:bravo>
</moo:alpha>;

AddTestCase("Calling setNamespace() on an attribute", correct, (MYXML = new XML(x1), MYXML.ns::bravo.@attr.setNamespace(ns2), MYXML));

correct = <moo:alpha xmlns:moo="http://moo/" xmlns:tar="http://tar/">
    <moo:bravo attr="1" xmlns="zoo">one</moo:bravo>
</moo:alpha>;

AddTestCase("Calling setNamespace() on an attribute with conflicting namespace prefix", correct.toString(), (MYXML = new XML(x1), MYXML.ns::bravo.@attr.setNamespace("zoo"), MYXML.toString()));

correct = <moo:alpha xmlns:moo="http://moo/" xmlns:tar="http://tar/">
    <moo:bravo attr="1" xmlns="moo">one</moo:bravo>
</moo:alpha>;

AddTestCase("Calling setNamespace() on an attribute with conflicting namespace prefix", correct.toString(), (MYXML = new XML(x1), MYXML.ns::bravo.@attr.setNamespace("moo"), MYXML.toString()));

correct = <moo:alpha xmlns:moo="http://moo/" xmlns:tar="http://tar/">
    <moo:bravo attr="1" xmlns="http://zoo/">one</moo:bravo>
</moo:alpha>;

AddTestCase("Calling setNamespace() on an attribute with conflicting namespace prefix", correct.toString(), (MYXML = new XML(x1), MYXML.ns::bravo.@attr.setNamespace("http://zoo/"), MYXML.toString()));

correct = <moo:alpha xmlns:moo="http://moo/" xmlns:tar="http://tar/">
    <moo:bravo noo:attr="1" xmlns:noo="http://mootar">one</moo:bravo>
</moo:alpha>;
delete correct;

AddTestCase("Calling setNamespace() on an attribute with conflicting namespace", correct.toString(), (MYXML = new XML(x1), MYXML.ns::bravo.@attr.setNamespace(ns3), MYXML.toString()));

var correct = <moo:alpha xmlns:moo="http://moo/" xmlns:tar="http://tar/">
    <moo:bravo moo:attr="1" xmlns:moo="http://bar/">one</moo:bravo>
</moo:alpha>;

AddTestCase("Calling setNamespace() on an attribute with conflicting namespace prefix", correct.toString(), (MYXML = new XML(x1), MYXML.ns::bravo.@attr.setNamespace(ns4), MYXML.toString()));

var correct = <moo:alpha xmlns:moo="http://moo/" xmlns:tar="http://tar/">
    <moo:bravo poo:attr="1" xmlns:poo="http://moo/">one</moo:bravo>
</moo:alpha>;

AddTestCase("Calling setNamespace() on an attribute with conflicting namespace", correct.toString(), (MYXML = new XML(x1), MYXML.ns::bravo.@attr.setNamespace(ns5), MYXML.toString()));


x1 = <alpha>
    <bravo attr="1">one</bravo>
</alpha>;

correct = <alpha>
    <bravo moo:attr="1" xmlns:moo="http://moo/">one</bravo>
</alpha>;

AddTestCase("Calling setNamespace() on an attribute with no existing namespace", correct.toString(), (MYXML = new XML(x1), MYXML.bravo.@attr.setNamespace(ns), MYXML.toString()));

	
END();
