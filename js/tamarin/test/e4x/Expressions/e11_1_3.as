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

START("11.1.3 - Wildcard Identifiers");

var x1 =
<alpha>
    <bravo>one</bravo>
    <charlie>two</charlie>
</alpha>

var correct = new XMLList("<bravo>one</bravo><charlie>two</charlie>");
TEST(1, correct, x1.*);

XML.ignoreWhitespace = false;

var xml1 = "<p><a>1</a><a>2</a><a>3</a></p>";
var xml2 = "<a>1</a><a>2</a><a>3</a>";
var xml3 = "<a><e f='a'>hey</e><e f='b'> </e><e f='c'>there</e></a><a>2</a><a>3</a>";
var xml4 = "<p><a hi='a' he='v' ho='m'>hello</a></p>";

var ns1 = new Namespace('foobar', 'http://boo.org');
var ns2 = new Namespace('fooboo', 'http://foo.org');
var ns3 = new Namespace('booboo', 'http://goo.org');

AddTestCase("x.a.*", "123", (x1 = new XML(xml1), x1.a.*.toString()));

AddTestCase("xmllist.a.*", "123", (x1 = new XMLList(xml1), x1.a.*.toString()));

AddTestCase("xmllist.*", "123", (x1 = new XMLList(xml2), x1.*.toString()));

AddTestCase("xmllist[0].*", "1", (x1 = new XMLList(xml2), x1[0].*.toString()));

AddTestCase("xmllist[0].e.*", "hey there", (x1 = new XMLList(xml3), x1[0].e.*.toString()));

AddTestCase("xml.a.@*", "avm", (x1 = new XML(xml4), x1.a.@*.toString()));


END();
