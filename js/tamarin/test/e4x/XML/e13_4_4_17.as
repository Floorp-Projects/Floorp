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

START("13.4.4.17 - XML inScopeNamespaces()");

//TEST(1, true, XML.prototype.hasOwnProperty("inScopeNamespaces"));
 
x1 =
<alpha xmlns:foo="http://foo/" xmlns:bar="http://bar/">
    <bravo>one</bravo>
</alpha>;

namespaces = x1.bravo.inScopeNamespaces();


TEST(2, "foo", namespaces[0].prefix);
TEST(3, "http://foo/", namespaces[0].uri);
TEST(4, "bar", namespaces[1].prefix);
TEST(5, "http://bar/", namespaces[1].uri);
//TEST(6, "", namespaces[2].prefix);
//TEST(7, "", namespaces[2].uri);
TEST(8, 2, namespaces.length);

var n1 = new Namespace('pfx', 'http://w3.org');
var n2 = new Namespace('http://us.gov');
var n3 = new Namespace('boo', 'http://us.gov');
var n4 = new Namespace('boo', 'http://hk.com');
var xml = "<a><b s='1'><c>55</c><d>bird</d></b><b s='2'><c>5</c><d>dinosaur</d></b></a>";


AddTestCase( "Two namespaces in toplevel scope:", ('http://hk.com'),
           (  x1 = new XML(xml), x1.addNamespace(n1), x1.addNamespace(n4), x1.inScopeNamespaces()[1].toString()));

AddTestCase( "Two namespaces in toplevel scope:", ('http://w3.org'),
           (  x1 = new XML(xml), x1.addNamespace(n1), x1.addNamespace(n4), x1.inScopeNamespaces()[0].toString()));

AddTestCase( "No namespace:", (''),
           (  x1 = new XML(xml), x1.inScopeNamespaces().toString()));
           
try {
	x1 = new XML(xml);
	x1.addNamespace();
	result = "no exception";
} catch (e1) {
	result = "exception";
}

AddTestCase( "Undefined namespace:", "exception", result);

AddTestCase( "Undefined namespace, length:", 1,
	   (  x1 = new XML(xml), x1.addNamespace(null), x1.inScopeNamespaces().length));
	   
AddTestCase( "One namespace w/o prefix, length:", 1,
	   (  x1 = new XML(xml), x1.addNamespace(n2), x1.inScopeNamespaces().length));

AddTestCase( "One namespace w/ prefix, length:", 1,
	   (  x1 = new XML(xml), x1.addNamespace(n1), x1.inScopeNamespaces().length));

AddTestCase( "One namespace at toplevel, one at child, length at toplevel:", 1,
	   (  x1 = new XML(xml), x1.addNamespace(n3), x1.b[0].c.addNamespace(n4), x1.inScopeNamespaces().length));

AddTestCase( "One namespace at toplevel, two at child, length at child:", 2,
	   (  x1 = new XML(xml), x1.addNamespace(n3), x1.b[1].c.addNamespace(n4), x1.b[1].c.addNamespace(n1), x1.b[1].c.inScopeNamespaces().length));

AddTestCase( "inScopeNamespaces[0].typeof:", "object",
           (  x1 = new XML(xml), x1.addNamespace(n1), x1.addNamespace(n4), typeof x1.inScopeNamespaces()[0]));

AddTestCase( "inScopeNamespaces[1].prefix:", "boo",
           (  x1 = new XML(xml), x1.addNamespace(n1), x1.addNamespace(n4), x1.inScopeNamespaces()[1].prefix));

   
var xmlDoc = "<?xml version=\"1.0\"?><xsl:stylesheet xmlns:xsl=\"http://www.w3.org/TR/xsl\"><b><c xmlns:foo=\"http://www.foo.org/\">hi</c></b></xsl:stylesheet>";

AddTestCase( "Reading one toplevel namespace:", (["http://www.w3.org/TR/xsl"]).toString(),
	   (  x1 = new XML(xmlDoc), x1.inScopeNamespaces().toString()));

AddTestCase( "Reading two namespaces up parent chain:", (["http://www.foo.org/","http://www.w3.org/TR/xsl"]).toString(),
	   (  x1 = new XML(xmlDoc), x1.b.c.inScopeNamespaces().toString()));

END();
