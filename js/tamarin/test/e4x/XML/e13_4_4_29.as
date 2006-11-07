/* ***** BEGIN LICENSE BLOCK ***** 
 Version: MPL 1.1/GPL 2.0/LGPL 2.1 

The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
"License"); you may not use this file except in compliance with the License. You may obtain 
a copy of the License at http://www.mozilla.org/MPL/ 

Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
language governing rights and limitations under the License. 

The Original Code is [Open Source Virtual Machine.] 

The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
by the Initial Developer are Copyright (C)[ 2005-2006 ] Adobe Systems Incorporated. All Rights 
Reserved. 

Contributor(s): Adobe AS3 Team

Alternatively, the contents of this file may be used under the terms of either the GNU 
General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
LGPL are applicable instead of those above. If you wish to allow use of your version of this 
file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
version of this file under the terms of the MPL, indicate your decision by deleting provisions 
above and replace them with the notice and other provisions required by the GPL or the 
LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
under the terms of any one of the MPL, the GPL or the LGPL. 

 ***** END LICENSE BLOCK ***** */
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

START("13.4.4.29 - XML prependChild()");

//TEST(1, true, XML.prototype.hasOwnProperty("prependChild"));

x1 = 
<alpha>
    <bravo>
        <charlie>one</charlie>
    </bravo>
</alpha>;

correct =
<alpha>
    <bravo>
        <foo>bar</foo>
        <charlie>one</charlie>
    </bravo>
</alpha>;

x1.bravo.prependChild(<foo>bar</foo>);

TEST(2, correct, x1);

emps =
<employees>
    <employee>
        <name>John</name>
    </employee>
    <employee>
        <name>Sue</name>
    </employee>
</employees>    

correct =
<employees>
    <employee>
        <prefix>Mr.</prefix>
        <name>John</name>
    </employee>
    <employee>
        <name>Sue</name>
    </employee>
</employees>    

emps.employee.(name == "John").prependChild(<prefix>Mr.</prefix>);

TEST(3, correct, emps);

XML.prettyPrinting = false;
var xmlDoc = "<company></company>";
var child1 = new XML("<employee id='1'><name>John</name></employee>");
var child2 = new XML("<employee id='2'><name>Sue</name></employee>");
var child3 = new XML("<employee id='3'><name>Bob</name></employee>");

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.prependChild(child1), MYXML.toString()", 
	"<company><employee id=\"1\"><name>John</name></employee></company>", 
	(MYXML = new XML(xmlDoc), MYXML.prependChild(child1), MYXML.toString()));

var MYXML = new XML(xmlDoc);
MYXML.prependChild(child2);

AddTestCase( "MYXML.prependChild(child1), MYXML.toString()", 
	"<company><employee id=\"1\"><name>John</name></employee><employee id=\"2\"><name>Sue</name></employee></company>", 
	(MYXML.prependChild(child1), MYXML.toString()));

MYXML = new XML(xmlDoc);
MYXML.prependChild(child3);
MYXML.prependChild(child2);

AddTestCase ("Making sure child added is a duplicate", true, (child2 === MYXML.children()[0]));
AddTestCase ("Making sure child added is a true copy", true, (child2 == MYXML.children()[0]));

AddTestCase( "MYXML.prependChild(child1), MYXML.toString()", 
	"<company><employee id=\"1\"><name>John</name></employee><employee id=\"2\"><name>Sue</name></employee><employee id=\"3\"><name>Bob</name></employee></company>", 
	(MYXML.prependChild(child1), MYXML.toString()));

AddTestCase( "MYXML.prependChild('simple text string'), MYXML.toString()", 
	"<company>simple text string<employee id=\"1\"><name>John</name></employee><employee id=\"2\"><name>Sue</name></employee><employee id=\"3\"><name>Bob</name></employee></company>", 
	(MYXML.prependChild("simple text string"), MYXML.toString()));

// !!@ test cases for adding comment nodes, PI nodes

MYXML = new XML(xmlDoc);
MYXML.prependChild(child3);
MYXML.prependChild(child2);
MYXML.prependChild(child1);

AddTestCase( "MYXML.prependChild('<!-- comment -->'), MYXML.toString()", 
	"<company><employee id=\"1\"><name>John</name></employee><employee id=\"2\"><name>Sue</name></employee><employee id=\"3\"><name>Bob</name></employee></company>", 
	(MYXML.prependChild("<!-- comment -->"), MYXML.toString()));

XML.ignoreComments = false;
MYXML = new XML(xmlDoc);
MYXML.prependChild(child3);
MYXML.prependChild(child2);
MYXML.prependChild(child1);

AddTestCase( "MYXML.prependChild('<!-- comment -->'), MYXML.toString()", 
	"<company><!-- comment --><employee id=\"1\"><name>John</name></employee><employee id=\"2\"><name>Sue</name></employee><employee id=\"3\"><name>Bob</name></employee></company>", 
	(MYXML.prependChild("<!-- comment -->"), MYXML.toString()));

MYXML = new XML(xmlDoc);
MYXML.prependChild(child3);
MYXML.prependChild(child2);
MYXML.prependChild(child1);

AddTestCase( "MYXML.prependChild('<?xml-stylesheet href=\"classic.xsl\" type=\"text/xml\"?>'), MYXML.toString()", 
	"<company><employee id=\"1\"><name>John</name></employee><employee id=\"2\"><name>Sue</name></employee><employee id=\"3\"><name>Bob</name></employee></company>", 
	(MYXML.prependChild("<?xml-stylesheet href=\"classic.xsl\" type=\"text/xml\"?>"), MYXML.toString()));

XML.ignoreProcessingInstructions = false;
MYXML = new XML(xmlDoc);
MYXML.prependChild(child3);
MYXML.prependChild(child2);
MYXML.prependChild(child1);

var expected = "TypeError: error: XML declaration may only begin entities.";
var result = "error, exception not thrown";

AddTestCase( "MYXML.prependChild('<?xml-stylesheet href=\"classic.xsl\" type=\"text/xml\"?>'), MYXML.toString()", 
	"<company><?xml-stylesheet href=\"classic.xsl\" type=\"text/xml\"?><employee id=\"1\"><name>John</name></employee><employee id=\"2\"><name>Sue</name></employee><employee id=\"3\"><name>Bob</name></employee></company>", 
	(MYXML.prependChild("<?xml-stylesheet href=\"classic.xsl\" type=\"text/xml\"?>"), MYXML.toString()));

try{

MYXML.prependChild("<?xml version=\"1.0\"?>");

} catch( e1 ){

result = e1.toString();

}
//Taking test out because bug 108406 has been deferred.
//AddTestCase("OPEN BUG: 108406 - MYXML.prependChild(\"<?xml version=\"1.0\"?>\")", expected, result);

// !!@ setting a property that starts with an "@" that implies an attribute name??
// Rhino throws an error

MYXML = new XML(xmlDoc);
MYXML.prependChild(child3);
MYXML.prependChild(child2);
MYXML.prependChild(child1);
AddTestCase( "MYXML.prependChild(\"<@notanattribute>hi</@notanattribute>\"), MYXML.toString()", 
	"<company><@notanattribute>hi</@notanattribute><employee id=\"1\"><name>John</name></employee><employee id=\"2\"><name>Sue</name></employee><employee id=\"3\"><name>Bob</name></employee></company>", 
	(MYXML.prependChild("<@notanattribute>hi</@notanattribute>"), MYXML.toString()));

MYXML = new XML('<LEAGUE></LEAGUE>');
x1 = new XMLList('<TEAM t="a">Giants</TEAM><TEAM t="b">Robots</TEAM>');
MYXML.prependChild(x1);
			
AddTestCase( "Prepend XMLList",
			'<LEAGUE><TEAM t="a">Giants</TEAM><TEAM t="b">Robots</TEAM></LEAGUE>',
			(MYXML.toString()) );
			
MYXML = new XML('<SCARY><MOVIE></MOVIE></SCARY>');
x1 = "poltergeist";
MYXML.MOVIE.prependChild(x1);
			
AddTestCase( "Prepend a string to child node",
			'<SCARY><MOVIE>poltergeist</MOVIE></SCARY>',
			(MYXML.toString()) );

// I believe the following two test cases are wrong. See bug 145184.
			
MYXML = new XML('<SCARY><MOVIE></MOVIE></SCARY>');
x1 = "poltergeist";
MYXML.prependChild(x1);
			
AddTestCase( "Prepend a string to top node",
			'<SCARY>poltergeist<MOVIE/></SCARY>',
			(MYXML.toString()) );
			
MYXML = new XML('<SCARY><MOVIE></MOVIE></SCARY>');
x1 = new XML("<the>poltergeist</the>");
MYXML.prependChild(x1);
			
AddTestCase( "Prepend a node to child node",
			'<SCARY><the>poltergeist</the><MOVIE/></SCARY>',
			(MYXML.toString()) );

var a = <a><b><c/></b></a>;

try {
	a.b.prependChild (a);
	result = a;
} catch (e1) {
	result = typeError(e1.toString());
}

AddTestCase("a = <a><b><c/></b></a>, a.b.prependChild(a)", "TypeError: Error #1118", result);



END();
