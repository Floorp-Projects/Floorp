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

START("13.4.4.5 - XML attributes()");

//TEST(1, true, XML.prototype.hasOwnProperty("attributes"));

x1 =
<alpha attr1="value1" attr2="value2" attr3="value3">
    <bravo>one</bravo>
</alpha>;

TEST(2, "xml", typeof(x1.attributes()));

// Iterate through the attributes of an XML value
x1 =
<alpha attr1="value1" attr2="value2" attr3="value3">
    <bravo>one</bravo>
</alpha>;

correct = new Array("value1", "value2", "value3");
i = 0;

for each (var a in x1.attributes())
{
    TEST_XML(i + 3, correct[i], a);
    i++;
}

var xmlDoc = "<TEAM foo = 'bar' two='second'>Giants</TEAM>";

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.attributes() instanceof XMLList", true, 
             (MYXML = new XML(xmlDoc), MYXML.attributes() instanceof XMLList ));
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.attributes() instanceof XML", false, 
             (MYXML = new XML(xmlDoc), MYXML.attributes() instanceof XML ));
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.attributes().length()", 2, 
             (MYXML = new XML(xmlDoc), MYXML.attributes().length() ));
XML.prettyPrinting = false;
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.attributes().toString()", "barsecond", 
             (MYXML = new XML(xmlDoc), MYXML.attributes().toString() ));
XML.prettyPrinting = true;
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.attributes().toString()", "barsecond", 
             (MYXML = new XML(xmlDoc), MYXML.attributes().toString() ));
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.attributes()[0].nodeKind()", "attribute", 
             (MYXML = new XML(xmlDoc), MYXML.attributes()[0].nodeKind() ));
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.attributes()[1].nodeKind()", "attribute", 
             (MYXML = new XML(xmlDoc), MYXML.attributes()[1].nodeKind() ));

END();
