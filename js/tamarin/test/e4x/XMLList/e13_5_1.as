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

START("13.5.1 - XMLList Constructor as Function");

x1 = XMLList();

TEST(1, "xml", typeof(x1));   
TEST(2, true, x1 instanceof XMLList);

// Make sure it's not copied if it's an XMLList
x1 = new XMLList("<alpha>one</alpha><bravo>two</bravo>");


y1 = XMLList(x1);
TEST(3, x1 === y1, true);

x1 += <charlie>three</charlie>;
TEST(4, x1 === y1, false);

// Load from one XML type
x1 = XMLList(<alpha>one</alpha>);
TEST_XML(5, "<alpha>one</alpha>", x1);

// Load from Anonymous
x1 = XMLList(<><alpha>one</alpha><bravo>two</bravo></>);
correct = new XMLList();
correct += <alpha>one</alpha>;
correct += <bravo>two</bravo>;
TEST_XML(6, correct.toString(), x1);

// Load from Anonymous as string
x1 = XMLList(<><alpha>one</alpha><bravo>two</bravo></>);
correct = new XMLList();
correct += <alpha>one</alpha>;
correct += <bravo>two</bravo>;
TEST_XML(7, correct.toString(), x1);

// Load from single textnode
x1 = XMLList("foobar");
TEST_XML(8, "foobar", x1);

x1 = XMLList(7);
TEST_XML(9, "7", x1);

// Undefined and null should behave like ""
x1 = XMLList(null);
TEST_XML(10, "", x1);

x1 = XMLList(undefined);
TEST_XML(11, "", x1);

XML.prettyPrinting = false;

var thisXML = "<TEAM>Giants</TEAM><CITY>San Francisco</CITY>";

// value is null
AddTestCase( "XMLList(null).toString()", "", XMLList(null).toString() );
AddTestCase( "typeof XMLList(null)", "xml", typeof XMLList(null) );
AddTestCase( "XMLList(null) instanceof XMLList", true, XMLList(null) instanceof XMLList);

// value is undefined
AddTestCase( "XMLList(undefined).toString()", "", XMLList(undefined).toString() );
AddTestCase( "typeof XMLList(undefined)", "xml", typeof XMLList(undefined) );
AddTestCase( "XMLList(undefined) instanceof XMLList", true, XMLList(undefined) instanceof XMLList);

// value is not supplied
AddTestCase( "XMLList().toString()", "", XMLList().toString() );
AddTestCase( "typeof XMLList()", "xml", typeof XMLList() );
AddTestCase( "XMLList() instanceof XMLList", true, XMLList() instanceof XMLList);

// value is supplied (string)
AddTestCase( "XMLList(thisXML).toString()", 
	"<TEAM>Giants</TEAM>" + NL() + "<CITY>San Francisco</CITY>", 
	XMLList(thisXML).toString() );
AddTestCase( "typeof XMLList(thisXML)", "xml", typeof XMLList(thisXML) );

// value is supplied (xmlList)
var xl = new XMLList ("<foo>bar></foo><foo2>bar></foo2>");
AddTestCase( "XMLList(xl) === xl", true, XMLList(xl) === xl);

// value is supplied (xml)
var x1 = new XML ("<foo>bar></foo>");
AddTestCase( "XMLList(x1).length()", 1, XMLList(x1).length());
AddTestCase( "XMLList(x1).contains(x1)", true, XMLList(x1)[0].contains(x1));

END();
