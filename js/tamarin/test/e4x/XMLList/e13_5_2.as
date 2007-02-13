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

START("13.5.2 - XMLList Constructor");

x1 = new XMLList();
TEST(1, "xml", typeof(x1));   
TEST(2, true, x1 instanceof XMLList);

// Load from another XMLList
// Make sure it is copied if it's an XMLList
x1 = 
<>
    <alpha>one</alpha>
    <bravo>two</bravo>
</>;

y1 = new XMLList(x1);

x1 += <charlie>three</charlie>;

TEST(3, "<alpha>one</alpha>" + NL() + "<bravo>two</bravo>", y1.toString());
   
// Load from one XML type
x1 = new XMLList(<alpha>one</alpha>);
TEST_XML(4, "<alpha>one</alpha>", x1);

// Load from Anonymous
x1 = new XMLList(<><alpha>one</alpha><bravo>two</bravo></>);
TEST(5, "<alpha>one</alpha>" + NL() + "<bravo>two</bravo>", x1.toString());

// Load from Anonymous as string
x1 = new XMLList(<><alpha>one</alpha><bravo>two</bravo></>);
TEST(6, "<alpha>one</alpha>" + NL() + "<bravo>two</bravo>", x1.toString());

// Load from single textnode
x1 = new XMLList("foobar");
TEST_XML(7, "foobar", x1);

x1 = XMLList(7);
TEST_XML(8, 7, x1);

// Undefined and null should behave like ""
x1 = new XMLList(null);
TEST_XML(9, "", x1);

x1 = new XMLList(undefined);
TEST_XML(10, "", x1);

var thisXML = "<TEAM>Giants</TEAM><CITY>San Francisco</CITY>";

// value is null
AddTestCase( "typeof new XMLList(null)", "xml", typeof new XMLList(null) );
AddTestCase( "new XMLList(null) instanceof XMLList", true, new XMLList(null) instanceof XMLList);
AddTestCase( "(new XMLList(null).length())", 0, (new XMLList(null)).length());
AddTestCase( "MYOB = new XMLList(null); MYOB.toString()", "",
             (MYOB = new XMLList(null), MYOB.toString(), MYOB.toString()) );

// value is undefined
AddTestCase( "typeof new XMLList(undefined)", "xml", typeof new XMLList(undefined) );
AddTestCase( "new XMLList(undefined) instanceof XMLList", true, new XMLList(undefined) instanceof XMLList);
AddTestCase( "(new XMLList(undefined).length())", 0, (new XMLList(undefined)).length());
AddTestCase( "MYOB = new XMLList(undefined); MYOB.toString()", "",
             (MYOB = new XMLList(undefined), MYOB.toString(), MYOB.toString()) );

// value is not supplied
AddTestCase( "typeof new XMLList()", "xml", typeof new XMLList() );
AddTestCase( "new XMLList() instanceof XMLList", true, new XMLList() instanceof XMLList);
AddTestCase( "(new XMLList().length())", 0, (new XMLList()).length());
AddTestCase( "MYOB = new XMLList(); MYOB.toString()", "",
             (MYOB = new XMLList(), MYOB.toString(), MYOB.toString()) );

//value is a number
AddTestCase( "typeof new XMLList(5)", "xml", typeof new XMLList(5) );
AddTestCase( "new XMLList(5) instanceof XMLList", true, new XMLList(5) instanceof XMLList);
AddTestCase( "(new XMLList(5).length())", 1, (new XMLList(5)).length());
AddTestCase( "MYOB = new XMLList(5); MYOB.toString()", "5",
             (MYOB = new XMLList(5), MYOB.toString(), MYOB.toString()) );
AddTestCase( "MYOB = new XMLList(5); MYOB.toXMLString()", "5",
             (MYOB = new XMLList(5), MYOB.toXMLString(), MYOB.toXMLString()) );

//value is 

// value is supplied 
XML.prettyPrinting = false;
AddTestCase( "typeof new XMLList(thisXML)", "xml", typeof new XMLList(thisXML) );
AddTestCase( "new XMLList(thisXML) instanceof XMLList", true, new XMLList(thisXML) instanceof XMLList);
AddTestCase( "(new XMLList(thisXML).length())", 2, (new XMLList(thisXML)).length());
AddTestCase( "MYOB = new XMLList(thisXML); MYOB.toString()", 
			"<TEAM>Giants</TEAM>" + NL() + "<CITY>San Francisco</CITY>",
             (MYOB = new XMLList(thisXML), MYOB.toString(), MYOB.toString()) );

// Strongly typed XMLList objects
var MYXML1:XMLList = new XMLList(thisXML);
AddTestCase("var MYXML:XMLList = new XMLList(\"<a>b</a><c>d</c>\");", new XMLList(thisXML).toString(), MYXML1.toString());

var MYXML2:XMLList = new XMLList(<><TEAM>Giants</TEAM><CITY>San Francisco</CITY></>);
AddTestCase("var MYXML:XMLList = new XMLList(<a>b</a><c>d</c>);", new XMLList(thisXML).toString(), MYXML2.toString());

var MYXML3:XMLList = <><TEAM>Giants</TEAM><CITY>San Francisco</CITY></>;
AddTestCase("var MYXML:XMLList = <><a>b</a><c>d</c></>;", new XMLList(thisXML).toString(), MYXML3.toString());

var MYXML4:XMLList = new XMLList();
AddTestCase("var MYXML:XMLList = new XMLList()", new XMLList(), MYXML4);

var MYXML5:XMLList = new XMLList(5);
AddTestCase("var MYXML:XMLList = new XMLList(5)", "5", MYXML5.toString());



END();
