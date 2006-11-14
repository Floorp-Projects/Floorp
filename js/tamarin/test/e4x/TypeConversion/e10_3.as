/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Igor Bukanov
 * Ethan Hugg
 * Milen Nankov
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

START("10.3 - toXML");

var x1;

// boolean
x1 = new Boolean(true);
TEST_XML(1, "true", new XML(x1));

// number
x1 = new Number(123);
TEST_XML(2, "123", new XML(x1));

// String
x1 = new String("<alpha><bravo>one</bravo></alpha>");
TEST(3, <alpha><bravo>one</bravo></alpha>, new XML(x1));

// XML
x1 = new XML("<alpha><bravo>one</bravo></alpha>");
TEST(4, <alpha><bravo>one</bravo></alpha>, new XML(x1));

// XMLList
x1 = new XMLList("<alpha><bravo>one</bravo></alpha>");
TEST(5, <alpha><bravo>one</bravo></alpha>, new XML(x1));

try {
    x1 = new XMLList(<alpha>one</alpha> + <bravo>two</bravo>);
    new XML(x1);
    SHOULD_THROW(6);
} catch (ex) {
    TEST(6, "TypeError", ex.name);
}
/*
// Undefined

try
{
	ToXML(undefined);
	AddTestCase( "ToXML(undefined) :", true, false );
}
catch (e)
{
	AddTestCase( "ToXML(undefined) :", true, true );
}


// Null

try
{
	ToXML(null);
	AddTestCase( "ToXML(null)      :", true, false );
}
catch (e)
{
	AddTestCase( "ToXML(null)      :", true, true );
}


// Boolean

var xt = "<parent xmlns=''>true</parent>";
var xf = "<parent xmlns=''>false</parent>";

AddTestCase( "ToXML(true)      :", true, (ToXML(true)==xt) );
AddTestCase( "ToXML(false)     :", true, (ToXML(false)==xf) );


// Number

var xn = "<parent xmlns=''>1234</parent>";

AddTestCase( "ToXML(1234)      :", true, (ToXML(1234)==xn) );


// XML

var x1 = new XML("<a><b>A</b></a>");

AddTestCase( "ToXML(XML)       :", true, (ToXML(x1)==x1) );


// XMLList

x1 = new XML("<a>A</a>");

AddTestCase( "ToXML(XMLList)   :", true, (ToXML(x1)=="A") );


// XMLList - XMLList contains more than one property

x1 = <a>A</a>
	<b>B</b>
	<c>C</c>;

try
{
	ToXML(x);
	AddTestCase( "ToXML(XMLList)   :", true, false );
}
catch (e)
{
	AddTestCase( "ToXML(XMLList)   :", true, true );
}


// Object

var a = new Array();

try
{
	ToXML(a);
	AddTestCase( "ToXML(Object)    :", true, false );
}
catch (e)
{
	AddTestCase( "ToXML(Object)    :", true, true );
}
*/
END();
