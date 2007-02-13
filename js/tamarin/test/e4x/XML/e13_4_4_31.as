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

START("13.4.4.31 - XML removeNamespace()");

//TEST(1, true, XML.prototype.hasOwnProperty("removeNamespace"));

x1 =
<alpha xmlns:foo="http://foo/">
    <bravo>one</bravo>
</alpha>;

correct =
<alpha>
    <bravo>one</bravo>
</alpha>;

TEST(1.5, correct, x1);
TEST(1.6, false, (correct.toString() == x1.toString()));

x1.removeNamespace("http://foo/");

TEST(2, correct.toString(), x1.toString());

TEST(2.5, correct, x1);

// Shouldn't remove namespace if referenced
x1 =
<foo:alpha xmlns:foo="http://foo/">
    <bravo>one</bravo>
</foo:alpha>;

correct =
<foo:alpha xmlns:foo="http://foo/">
    <bravo>one</bravo>
</foo:alpha>;

x1.removeNamespace("http://foo/");

TEST(3, correct, x1);


var xmlDoc = "<?xml version=\"1.0\"?><xsl:stylesheet xmlns:xsl=\"http://www.w3.org/TR/xsl\"><b><c xmlns:foo=\"http://www.foo.org/\">hi</c></b></xsl:stylesheet>";
var ns1 = Namespace ("xsl", "http://www.w3.org/TR/xsl");
var ns2 = Namespace ("foo", "http://www.foo.org");


// Namespace that is referenced by QName should not be removed
AddTestCase( "MYXML.removeNamespace(QName reference)", "http://www.w3.org/TR/xsl", 
	   (  MYXML = new XML(xmlDoc), MYXML.removeNamespace('xsl'), myGetNamespace(MYXML, 'xsl').toString()));


// Other namespaces should be removed
AddTestCase( "MYXML.removeNamespace(no Qname reference)", undefined, 
	   (  MYXML = new XML(xmlDoc), MYXML.b.c.removeNamespace('foo'), myGetNamespace(MYXML, 'foo')) );

var n1 = new Namespace('pfx', 'http://w3.org');
var n2 = new Namespace('http://us.gov');
var n3 = new Namespace('boo', 'http://us.gov');
var n4 = new Namespace('boo', 'http://hk.com');
var xml = "<a><b s='1'><c>55</c><d>bird</d></b><b s='2'><c>5</c><d>dinosaur</d></b></a>";

AddTestCase("Two namespaces in one node", true,
           ( x1 = new XML(xml), x1.addNamespace(n1), x1.addNamespace(n3), x1.removeNamespace(n3),
             x1.removeNamespace(n1), (myGetNamespace(x1, 'pfx') == myGetNamespace(x1, 'boo'))) );

AddTestCase("Two namespaces in one node", 1,
           ( x1 = new XML(xml), x1.addNamespace(n1), x1.addNamespace(n3), x1.removeNamespace(n3),
             x1.removeNamespace(n1), x1.inScopeNamespaces().length) );
             
AddTestCase("Two namespace in two different nodes", undefined,
           ( x1 = new XML(xml), x1.addNamespace(n3), x1.b[0].c.addNamespace(n1),
             x1.removeNamespace(n1), myGetNamespace(x1.b[0].c, 'pfx')));
             
var xml1 = <a xmlns:n2="http://us.gov"><b s='1'><c>55</c><d>bird</d></b><b s='2'><c>5</c><d>dinosaur</d></b></a>;
var xml2 = <a><b s='1'><c>55</c><d>bird</d></b><b s='2'><c>5</c><d>dinosaur</d></b></a>;
             
AddTestCase("Remove namespace without prefix", xml2,
             xml1.removeNamespace(n2), xml1);


END();
