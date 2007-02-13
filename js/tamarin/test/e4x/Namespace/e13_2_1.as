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

START("13.2.1 - Namespace Constructor as Function");

n = Namespace();
m = new Namespace();
TEST(1, typeof(m), typeof(n));
TEST(2, m.prefix, n.prefix);
TEST(3, m.uri, n.uri);

n = Namespace("http://foobar/");
m = new Namespace("http://foobar/");
TEST(4, typeof(m), typeof(n));
TEST(5, m.prefix, n.prefix);
TEST(6, m.uri, n.uri);

n = Namespace("foobar", "http://foobar/");
m = new Namespace("foobar", "http://foobar/");
TEST(7, typeof(m), typeof(n));
TEST(8, m.prefix, n.prefix);
TEST(9, m.uri, n.uri);

n = Namespace(m);
TEST(10, m, n);

var thisXML = "<XML><TEAM>Giants</TEAM><CITY>San Francisco</CITY></XML>";
var NULL_OBJECT = "";
 

// value is not supplied
AddTestCase( "Namespace()", NULL_OBJECT, Namespace().toString());
AddTestCase( "typeof Namespace()", "object", typeof Namespace() );
//AddTestCase( "Namespace().__proto__ == Namespace.prototype", true, Namespace().__proto__ == Namespace.prototype);

//One value is supplied 
AddTestCase( "Namespace('pfx').toString()", 'pfx', Namespace('pfx').toString() );
AddTestCase( "typeof Namespace('pfx')", "object", typeof Namespace('pfx') );
//AddTestCase( "Namespace('pfx').__proto__ == Namespace.prototype", true, Namespace('pfx').__proto__ == Namespace.prototype);

var ns = new Namespace('http://foo.bar');
AddTestCase( "Namespace(nsObj).toString()", 'http://foo.bar', Namespace(ns).toString() );

//Two values are supplied 
AddTestCase( "Namespace('pfx','ns') == new Namespace('pfx', 'ns')", new Namespace('pfx', 'http://www.w3.org/TR/html4/'), Namespace('pfx','http://www.w3.org/TR/html4/') );
AddTestCase( "typeof Namespace('pfx','http://www.w3.org/TR/html4/')", "object", typeof Namespace('pfx','http://www.w3.org/TR/html4/') );
//AddTestCase( "Namespace('pfx','http://www.w3.org/TR/html4/').__proto__ == Namespace.prototype", true, Namespace('pfx','http://www.w3.org/TR/html4/').__proto__ == Namespace.prototype );

ns = new Namespace('pfx', 'http://foo.bar');
AddTestCase( "Namespace(nsObj).toString()", 'http://foo.bar', Namespace(ns).toString() );

END();
