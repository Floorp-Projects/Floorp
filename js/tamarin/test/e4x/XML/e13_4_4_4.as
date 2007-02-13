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

START("13.4.4.4 - XML attribute()");

//TEST(1, true, XML.prototype.hasOwnProperty("attribute"));

// Get count of employees
emps =
<employees count="2">
    <employee id="0"><name>Jim</name><age>25</age></employee>
    <employee id="1"><name>Joe</name><age>20</age></employee>
</employees>;

TEST_XML(2, 2, emps.attribute("count"));

// Get the id of the employee age 25
emps =
<employees>
    <employee id="0"><name>Jim</name><age>25</age></employee>
    <employee id="1"><name>Joe</name><age>20</age></employee>
</employees>;

TEST_XML(3, 0, emps.employee.(age == "25").attribute("id"));

// Get the id of the employee named Jim
emps =
<employees>
    <employee id="0"><name>Jim</name><age>25</age></employee>
    <employee id="1"><name>Joe</name><age>20</age></employee>
</employees>;

TEST_XML(4, 0, emps.employee.(name == "Jim").attribute("id"));

var xmlDoc = "<TEAM foo = 'bar' two='second'>Giants</TEAM>";

// verify that attribute correct finds one attribute node
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.attribute('foo') instanceof XMLList", true, 
             (MYXML = new XML(xmlDoc), MYXML.attribute('foo') instanceof XMLList ));
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.attribute('foo') instanceof XML", false, 
             (MYXML = new XML(xmlDoc), MYXML.attribute('foo') instanceof XML ));
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.attribute('foo').length()", 1, 
             (MYXML = new XML(xmlDoc), MYXML.attribute('foo').length() ));
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.attribute('foo').toString()", "bar", 
             (MYXML = new XML(xmlDoc), MYXML.attribute('foo').toString() ));
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.attribute('foo')[0].nodeKind()", "attribute", 
             (MYXML = new XML(xmlDoc), MYXML.attribute('foo')[0].nodeKind() ));

// verify that attribute doesn't find non-existent names
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.attribute('DOESNOTEXIST') instanceof XMLList", true, 
             (MYXML = new XML(xmlDoc), MYXML.attribute('DOESNOTEXIST') instanceof XMLList ));
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.attribute('DOESNOTEXIST') instanceof XML", false, 
             (MYXML = new XML(xmlDoc), MYXML.attribute('DOESNOTEXIST') instanceof XML ));
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.attribute('DOESNOTEXIST').length()", 0, 
             (MYXML = new XML(xmlDoc), MYXML.attribute('DOESNOTEXIST').length() ));
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.attribute('DOESNOTEXIST').toString()", "", 
             (MYXML = new XML(xmlDoc), MYXML.attribute('DOESNOTEXIST').toString() ));
             
// verify that attribute doesn't find child node names             
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.attribute('TEAM') instanceof XMLList", true, 
             (MYXML = new XML(xmlDoc), MYXML.attribute('TEAM') instanceof XMLList ));
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.attribute('TEAM') instanceof XML", false, 
             (MYXML = new XML(xmlDoc), MYXML.attribute('TEAM') instanceof XML ));
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.attribute('TEAM').toString()", "", 
             (MYXML = new XML(xmlDoc), MYXML.attribute('TEAM').toString() ));
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.attribute('TEAM').length()", 0, 
             (MYXML = new XML(xmlDoc), MYXML.attribute('TEAM').length() ));

xl = <x a="aatr" b="batr">y</x>;

AddTestCase( "attribute(new QName(\"*\"))", "aatrbatr", 
	   ( q = new QName("*"), xl.attribute(q).toString() ));	 

AddTestCase( "attribute(new QName(\"@*\"))", "", 
	   ( q = new QName("@*"), xl.attribute(q).toString() ));	 

END();
