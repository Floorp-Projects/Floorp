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

START("11.1.5 - XMLList Initializer");

XML.ignoreWhitespace = true;

var docfrag = <><fName>Phil</fName><age>35</age><hobby>skiing</hobby></>;
TEST(1, "xml", typeof(docfrag));

var correct = <fName>Phil</fName>;
TEST(2, correct, docfrag[0]);

var emplist = <>
          <employee id="0"><name>Jim</name><age>25</age></employee>
          <employee id="1"><name>Joe</name><age>20</age></employee>
          <employee id="2"><name>Sue</name><age>30</age></employee>
          </>;  
          
TEST(3, "xml", typeof(emplist));
TEST_XML(4, 2, emplist[2].@id);

var myVar = "kitty";

var xml1 = <><a><b><c>1</c></b></a></>;
var xml2 = <>a</>;
var xml3 = <><a>b</a></>;
var xml4 = <><a b='1'><c>cat</c><d>walk</d></a><a b='2'><c>dog</c><d>run</d></a></>;
var xml5 = <></>;
var xml6 = <><{myVar}>hello</{myVar}></>;
var xml7 = <><c q='1'>"quotes"</c><c q='2'>&#x7B;curly brackets&#x7D;</c></>;
var xml8 = <><a>5 &gt; 4</a></>;
var empxml = <>
	<employee id="0" ><name>Jim</name><age>25</age></employee>
	<employee id="1" ><name>Joe</name><age>20</age></employee>
	<employee id="2" ><name>Sue</name><age>30</age></employee>
</>;
emplist = "<employee id=\"0\" ><name>Jim</name><age>25</age></employee><employee id=\"1\" ><name>Joe</name><age>20</age></employee><employee id=\"2\" ><name>Sue</name><age>30</age></employee>";


AddTestCase( "<><a><b><c>1</c></b></a></> == new XMLList(\"<a><b><c>1</c></b></a>\")", true, 
           (  x1 = new XMLList('<a><b><c>1</c></b></a>'), (xml1 == x1)));
           
AddTestCase( "<>a</> == new XMLList('a')", true, 
           (  x1 = new XMLList('a'), (xml2 == x1)));

AddTestCase( "<><a>b</a></> == new XMLList('<a>b</a>')", true, 
           (  x1 = new XMLList('<a>b</a>'), (xml3 == x1)));

AddTestCase( "<>[list]</> == new XMLList([list])", true, 
           (  x1 = new XMLList('<a b="1"><c>cat</c><d>walk</d></a><a b="2"><c>dog</c><d>run</d></a>'), (xml4 == x1)));
           
AddTestCase( "<></> == new XMLList()", true, 
	   (  x1 = new XMLList(), (xml5 == x1)));

AddTestCase( "<></> == new XMLList(\"\")", true, 
	   (  x1 = new XMLList(""), (xml5 == x1)));
           
AddTestCase( "<><{myVar}>hello</{myVar}></> == new XMLList('<value>hello</value>')", true, 
	   (  x1 = new XMLList("<kitty>hello</kitty>"), (xml6 == x1)));

AddTestCase( "<><c>&#x7B; \\\"\\\" &#x7B;</c></> == new XMLList(<c>{ \"\" }</c>)", true, 
	   (  x1 = new XMLList("<c q='1'>\"quotes\"</c><c q='2'>{curly brackets}</c>"), (xml7 == x1)));
	   
AddTestCase( "<><a>5 &gt; 4</a></> == new XMLList('<a>5 > 4</a>')", true, 
	   (  x1 = new XMLList("<a>5 > 4</a>"), (xml8 == x1)));
	   
AddTestCase( "Multiline XML", true, 
	   (  x1 = new XMLList(emplist), (empxml == x1)));
	   
// Testing for extra directives. See bug 94230.
var xl = <><?xml version="1.0" encoding="UTF-8"?>
<?mso-infoPathSolution solutionVersion="1.0.0.26" productVersion="11.0.6250" PIVersion="1.0.0.0" href="file:///C:\Documents%20and%20Settings\Bob\BoB\Goodbye%20Doubt\Repository\CMS\Forms\PatternForm.xsn" name="urn:schemas-microsoft-com:office:infopath:PatternForm:urn-axiology-PatternDocument" language="en-us" ?>
<?mso-application progid="InfoPath.Document"?><root><blah>a</blah></root></>;
	   
AddTestCase("Testing for extra directives", <><root><blah>a</blah></root></>, xl);

xl = new XMLList("<?xml version=\"1.0\" encoding=\"UTF-8\"?><?mso-infoPathSolution solutionVersion=\"1.0.0.26\" productVersion=\"11.0.6250\" PIVersion=\"1.0.0.0\" href=\"file:///C:\Documents%20and%20Settings\Bob\BoB\Goodbye%20Doubt\Repository\CMS\Forms\PatternForm.xsn\" name=\"urn:schemas-microsoft-com:office:infopath:PatternForm:urn-axiology-PatternDocument\" language=\"en-us\" ?><?mso-application progid=\"InfoPath.Document\"?><root><blah>a</blah></root>");

AddTestCase("Testing for extra directives", new XMLList("<root><blah>a</blah></root>"), xl);
           
END();
