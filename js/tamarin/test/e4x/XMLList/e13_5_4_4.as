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

START("13.5.4.4 - XMLList child()");

//TEST(1, true, XMLList.prototype.hasOwnProperty("child"));

// Test with XMLList of size 0
x1 = new XMLList()
TEST(2, "xml", typeof(x1.child("bravo")));
TEST_XML(3, "", x1.child("bravo"));

// Test with XMLList of size 1
x1 += <alpha>one<bravo>two</bravo></alpha>;    
TEST(4, "xml", typeof(x1.child("bravo")));
TEST_XML(5, "<bravo>two</bravo>", x1.child("bravo"));

x1 += <charlie><bravo>three</bravo></charlie>;
TEST(6, "xml", typeof(x1.child("bravo")));

correct = <><bravo>two</bravo><bravo>three</bravo></>;
TEST(7, correct, x1.child("bravo"));

// Test no match, null and undefined
TEST(8, "xml", typeof(x1.child("foobar")));
TEST_XML(9, "", x1.child("foobar"));

try {
  x1.child(null);
  SHOULD_THROW(10);
} catch (ex) {
  TEST(10, "TypeError", ex.name);
}

// Test numeric inputs
x1 = 
<alpha>
    <bravo>one</bravo>
    <charlie>two</charlie>
</alpha>;

TEST(12, <bravo>one</bravo>, x1.child(0));
TEST(13, <charlie>two</charlie>, x1.child(1));

var xmlDoc = "<MLB><Team>Giants</Team><City>San Francisco</City></MLB><MLB2><Team>Padres</Team><City>San Diego</City></MLB2>";

// propertyName as a string
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.child('Team')", "<Team>Giants</Team>" + NL() + "<Team>Padres</Team>", 
             (MYXML = new XMLList(xmlDoc), MYXML.child('Team').toString() ));
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.child('Team') instanceof XMLList", true, 
             (MYXML = new XMLList(xmlDoc), MYXML.child('Team') instanceof XMLList ));
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.child('Team') instanceof XML", false, 
             (MYXML = new XMLList(xmlDoc), MYXML.child('Team') instanceof XML ));
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.child('Team').length()", 2, 
             (MYXML = new XMLList(xmlDoc), MYXML.child('Team').length()));
AddTestCase( "MYXML = new XMLList(null), MYXML.child('Team')", "", 
             (MYXML = new XMLList(null), MYXML.child('Team').toString() ));
AddTestCase( "MYXML = new XMLList(undefined), MYXML.child('Team')", "", 
             (MYXML = new XMLList(undefined), MYXML.child('Team').toString() ));
AddTestCase( "MYXML = new XMLList(), MYXML.child('Team')", "", 
             (MYXML = new XMLList(), MYXML.child('Team').toString() ));

// propertyName as a numeric index
// !!@ doesn't work in Rhino. Should this return the 1st child (from 0th) 
// of the MLB node which should be "San Francisco"
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.child(1) instanceof XMLList", true, 
             (MYXML = new XMLList(xmlDoc), MYXML.child(1) instanceof XMLList ));
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.child(1)", "<City>San Francisco</City>" + NL() + "<City>San Diego</City>", 
             (MYXML = new XMLList(xmlDoc), MYXML.child(1).toString() ));
AddTestCase( "MYXML = new XMLList(null), MYXML.child(1)", "", 
             (MYXML = new XMLList(null), MYXML.child(1).toString() ));
AddTestCase( "MYXML = new XMLList(undefined), MYXML.child(1)", "", 
             (MYXML = new XMLList(undefined), MYXML.child(1).toString() ));
AddTestCase( "MYXML = new XMLList(), MYXML.child(1)", "", 
             (MYXML = new XMLList(), MYXML.child(1).toString() ));

AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.child(0) instanceof XMLList", true, 
             (MYXML = new XMLList(xmlDoc), MYXML.child(0) instanceof XMLList ));
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.child(0)", "<Team>Giants</Team>" + NL() + "<Team>Padres</Team>", 
             (MYXML = new XMLList(xmlDoc), MYXML.child(0).toString() ));
AddTestCase( "MYXML = new XMLList(null), MYXML.child(0)", "", 
             (MYXML = new XMLList(null), MYXML.child(0).toString() ));
AddTestCase( "MYXML = new XMLList(undefined), MYXML.child(0)", "", 
             (MYXML = new XMLList(undefined), MYXML.child(0).toString() ));
AddTestCase( "MYXML = new XMLList(), MYXML.child(0)", "", 
             (MYXML = new XMLList(), MYXML.child(0).toString() ));

// propertyName is invalid

// invalid propertyName
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.child('DoesNotExist') instanceof XMLList", true, 
             (MYXML = new XMLList(xmlDoc), MYXML.child('DoesNotExist') instanceof XMLList ));
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.child('DoesNotExist')", "", 
             (MYXML = new XMLList(xmlDoc), MYXML.child('DoesNotExist').toString() ));
AddTestCase( "MYXML = new XMLList(null), MYXML.child('DoesNotExist')", "", 
             (MYXML = new XMLList(null), MYXML.child('DoesNotExist').toString() ));
AddTestCase( "MYXML = new XMLList(undefined), MYXML.child('DoesNotExist')", "", 
             (MYXML = new XMLList(undefined), MYXML.child('DoesNotExist').toString() ));
AddTestCase( "MYXML = new XMLList(), MYXML.child('DoesNotExist')", "", 
             (MYXML = new XMLList(), MYXML.child('DoesNotExist').toString() ));

// invalid index
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.child(8) instanceof XMLList", true, 
             (MYXML = new XMLList(xmlDoc), MYXML.child(8) instanceof XMLList ));
AddTestCase( "MYXML = new XMLList(xmlDoc), MYXML.child(8)", "", 
             (MYXML = new XMLList(xmlDoc), MYXML.child(8).toString() ));
AddTestCase( "MYXML = new XMLList(null), MYXML.child(8)", "", 
             (MYXML = new XMLList(null), MYXML.child(8).toString() ));
AddTestCase( "MYXML = new XMLList(undefined), MYXML.child(8)", "", 
             (MYXML = new XMLList(undefined), MYXML.child(8).toString() ));
AddTestCase( "MYXML = new XMLList(), MYXML.child(8)", "", 
             (MYXML = new XMLList(), MYXML.child(8).toString() ));

END();
