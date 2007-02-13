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

START("13.3.2 - QName Constructor");

q = new QName("*");
TEST(1, "object", typeof(q));
TEST(2, "*", q.localName);
TEST(3, null, q.uri);
TEST(4, "*::*", q.toString());

// Default namespace
q = new QName("foobar");
TEST(5, "object", typeof(q));
TEST(6, "foobar", q.localName);
TEST(7, "", q.uri);
TEST(8, "foobar", q.toString());

q = new QName("http://foobar/", "foobar");
TEST(9, "object", typeof(q));
TEST(10, "foobar", q.localName);
TEST(11, "http://foobar/", q.uri);
TEST(12, "http://foobar/::foobar", q.toString());

p = new QName(q);
TEST(13, typeof(p), typeof(q));
TEST(14, q.localName, p.localName);
TEST(15, q.uri, p.uri);

n = new Namespace("http://foobar/");
q = new QName(n, "foobar");
TEST(16, "object", typeof(q));

q = new QName(null);
TEST(17, "object", typeof(q));
TEST(18, "null", q.localName);
TEST(19, "", q.uri);
TEST(20, "null", q.toString());

q = new QName(null, null);
TEST(21, "object", typeof(q));
TEST(22, "null", q.localName);
TEST(23, null, q.uri);
TEST(24, "*::null", q.toString());

q = new QName("attr1");
q2 = new QName(q, "attr1");
q3 = QName(q);

TEST(25, "attr1", q.toString());
TEST(26, "attr1", q2.toString());
TEST(27, "attr1", q3.toString());

q = new QName(n, "attr1");
q2 = new QName(q, "attr1");

TEST(28, "http://foobar/::attr1", q.toString());
TEST(29, "http://foobar/::attr1", q2.toString());

// no value is supplied
AddTestCase( "ns = new QName()", "", (ns = new QName(), ns.localName) );

// one value is supplied
AddTestCase( "typeof new QName('name')", 'object', typeof new QName('name') );
AddTestCase( "new QName('name') instanceof QName", true, new QName('name') instanceof QName);
AddTestCase( "new QName('name') == 'name'", true, new QName('name') == 'name');
AddTestCase( "ns = new QName('name'), ns.uri == ''", true, 
	(ns = new QName('name'), ns.uri == '') );
AddTestCase( "ns = new QName('name'), ns.uri == null", false, 
	(ns = new QName('name'), ns.uri == null) );
AddTestCase( "ns = new QName('name'), ns.uri == undefined", false, 
	(ns = new QName('name'), ns.uri == undefined) );
AddTestCase( "ns = new QName('name'), typeof ns.uri", 'string', 
	(ns = new QName('name'), typeof ns.uri) );
AddTestCase( "ns = new QName('name'), ns.localName == 'name'", true, 
	(ns = new QName('name'), ns.localName == 'name') );
AddTestCase( "ns = new QName(undefined)", "", (ns = new QName(undefined), ns.localName) );
AddTestCase( "ns = new QName('')", "", (ns = new QName(""), ns.localName) );
AddTestCase( "MYOB = new QName('nameofobj'),typeof new QName(MYOB)", 
	'object', 
	(MYOB = new QName('nameofobj'), typeof new QName(MYOB)) );


//two values are supplied
AddTestCase( "MYOB = new QName(null, 'nameofobj'); MYOB.toString()", 
			"*::nameofobj",
             (MYOB = new QName(null, 'nameofobj'), MYOB.toString() ));

AddTestCase( "MYOB = new QName(null, 'nameofobj'); MYOB.uri", null,
             (MYOB = new QName(null, 'nameofobj'), MYOB.uri) );

AddTestCase( "MYOB = new QName(null, 'nameofobj'); MYOB.localName", 'nameofobj',
             (MYOB = new QName(null, 'nameofobj'), MYOB.localName) );
AddTestCase( "MYOB = new QName('namespace', undefined); MYOB.localName", "",
             (MYOB = new QName('namespace', undefined), MYOB.localName) );

AddTestCase( "MYOB = new QName('namespace', ''); MYOB.localName", "",
             (MYOB = new QName('namespace', ""), MYOB.localName) );
             
x1 =
<alpha>
    <bravo attr1="value1" ns:attr1="value3" xmlns:ns="http://someuri">
        <charlie attr1="value2" ns:attr1="value4"/>
    </bravo>
</alpha>
             
y = <ns:attr1 xmlns:ns="http://someuri"/>
q3 = y.name();

AddTestCase("q3 = y.name()", "http://someuri::attr1", q3.toString());
AddTestCase("x1.bravo.@[q3]", new XML("value3"), x1.bravo.@[q3]);


END();
