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

START("11.1.4 - XML Initializer");

XML.ignoreWhitespace = true;
person = <person><name>John</name><age>25</age></person>;
TEST(1, <person><name>John</name><age>25</age></person>, person);    

e = <employees>
    <employee id = "1"><name>Joe</name><age>20</age></employee>
    <employee id = "2"><name>Sue</name><age>30</age></employee>
    </employees>;

TEST_XML(2, 1, e.employee[0].@id);

correct = <name>Sue</name>;
TEST(3, correct, e.employee[1].name);

names = new Array();
names[0] = "Alpha";
names[1] = "Bravo";
names[2] = "Charlie";
names[3] = "Delta";
names[4] = "Echo";
names[5] = "Golf";
names[6] = "Hotel";
names[7] = "India";
names[8] = "Juliet";
names[9] = "Kilo";

ages = new Array();
ages[0] = "20";
ages[1] = "21";
ages[2] = "22";
ages[3] = "23";
ages[4] = "24";
ages[5] = "25";
ages[6] = "26";
ages[7] = "27";
ages[8] = "28";
ages[9] = "29";

for (i = 0; i < 10; i++)
{
    e.*[i] = <employee id={i}>
           <name>{names[i].toUpperCase()}</name>
           <age>{ages[i]}</age>
           </employee>;

    correct = new XML("<employee id=\"" + i + "\"><name>" + names[i].toUpperCase() + "</name><age>" + ages[i] + "</age></employee>");
    TEST(4 + i, correct, e.*[i]);
}

tagName = "name";
attributeName = "id";
attributeValue = 5;
content = "Fred";

x1 = <{tagName} {attributeName}={attributeValue}>{content}</{tagName}>;
TEST(14, "<name id=\"5\">Fred</name>", x1.toXMLString());



// Test {} on XML and XMLList types
x1 = 
<rectangle>
    <length>30</length>
    <width>50</width>
</rectangle>;

correct =
<rectangle>
    <width>50</width>
    <length>30</length>
</rectangle>;

x1 = <rectangle>{x1.width}{x1.length}</rectangle>;

TEST(15, correct, x1);

var content = "<foo name=\"value\">bar</foo>";

x1 = <x><a>{content}</a></x>;
correct = <x/>;
correct.a = content;
TEST(16, correct, x1);

x1 = <x a={content}/>;
correct = <x/>;
correct.@a = content;
TEST(17, correct, x1);

a = 5;
b = 3;
c = "x";
x1 = <{c} a={a + " < " + b + " is " + (a < b)}>{a + " < " + b + " is " + (a < b)}</{c}>;
TEST(18, "<x a=\"5 &lt; 3 is false\">5 &lt; 3 is false</x>", x1.toXMLString());

x1 = <{c} a={a + " > " + b + " is " + (a > b)}>{a + " > " + b + " is " + (a > b)}</{c}>;
TEST(19, "<x a=\"5 > 3 is true\">5 &gt; 3 is true</x>", x1.toXMLString());

var tagname = "name";
var attributename = "id";
var attributevalue = 5;
content = "Fred";
 
var xml1 = <{tagname} {attributename}={attributevalue}>{content}</{tagname}>;

AddTestCase( "x = <{tagname} {attributename}={attributevalue}>{content}</{tagname}>", true, 
           (  x1 = new XML('<name id="5">Fred</name>'), (xml1 == x1)));
           


names = ["Alfred", "Allie", "Maryann", "Jason", "Amy", "Katja", "Antonio", "Melvin", "Stefan", "Amber"];
ages = [55, 21, 25, 23, 28, 30, 35, 26, 30, 30];

var xml2;
var xml2string = "<employees>";
var e = new Array();
for (i = 0; i < 10; i++) {
	e[i] = <employee id={i}>			
		<name>{names[i].toUpperCase()}</name>	
		<age>{ages[i]}</age>			
	</employee>;
	xml2string += "<employee id=\"" + i + "\"><name>" + names[i].toUpperCase() + "</name><age>" + ages[i] + "</age></employee>";
}
xml2 = <employees>{e[0]}{e[1]}{e[2]}{e[3]}{e[4]}{e[5]}{e[6]}{e[7]}{e[8]}{e[9]}</employees>;
xml2string += "</employees>";

AddTestCase( "Evaluating expressions in a for loop", true, 
           (  x1 = new XML(xml2string), (xml2 == x1)));
          

var xml3 = <person><name>John</name><age>25</age></person>;

AddTestCase( "x = <person><name>John</name><age>25</age></person>", true, 
           (  x1 = new XML(xml3.toString()), (xml3 == x1)));

var xml4 = new XML("<xml><![CDATA[<hey>]]></xml>");

AddTestCase( "<xml><![CDATA[<hey>]]></xml>", true, 
           (  x1 = new XML("<xml><![CDATA[<hey>]]></xml>"), (xml4 == x1)));
          
xml5 = <xml><b>heh            hey</b></xml>;
XML.ignoreWhitespace = true;

AddTestCase( "<xml><b>heh            hey</b></xml>", true, 
           (  x1 = new XML("<xml><b>heh            hey</b></xml>"), (xml5 == x1)));


AddTestCase( "x = new XML(\"\"), xml = \"\", (xml == x)", true, 
           (  x1 = new XML(""), xml = "", (xml == x1)));
           

       
var xx = new XML("<classRec><description><![CDATA[characteristics:<ul> <li>A</li> <li>B</li>   </ul>]]></description></classRec>");

AddTestCase( "<xml><![CDATA[characteristics:<ul> <li>A</li> <li>B</li>   </ul>]]></xml>, xml.toXMLString()",
             "<classRec>" + NL() + "  <description><![CDATA[characteristics:<ul> <li>A</li> <li>B</li>   </ul>]]></description>" + NL() + "</classRec>",
             xx.toXMLString());
             
AddTestCase( "<xml><![CDATA[characteristics:<ul> <li>A</li> <li>B</li>   </ul>]]></xml>, xml.description.text()",
             "characteristics:<ul> <li>A</li> <li>B</li>   </ul>",
             xx.description.text().toString());

AddTestCase( "<xml><![CDATA[characteristics:<ul> <li>A</li> <li>B</li>   </ul>]]></xml>, xml.description.child(0)",
             "characteristics:<ul> <li>A</li> <li>B</li>   </ul>",
             xx.description.child(0).toString());
             
AddTestCase( "<xml><![CDATA[characteristics:<ul> <li>A</li> <li>B</li>   </ul>]]></xml>, xml.description.child(0).nodeKind()",
             "text",
             xx.description.child(0).nodeKind());
             
var desc = "this is the <i>text</i>";

x1 = <description>{"<![CDATA[" + desc + "]]>"}</description>;

AddTestCase("desc = \"this is the <i>text</i>\"; x = <description>{\"<![CDATA[\" + desc + \"]]>\"}</description>;",
            "<![CDATA[this is the <i>text</i>]]>", x1.toString());


AddTestCase("desc = \"this is the <i>text</i>\"; x = <description>{\"<![CDATA[\" + desc + \"]]>\"}</description>;",
            "<description>&lt;![CDATA[this is the &lt;i&gt;text&lt;/i&gt;]]&gt;</description>", x1.toXMLString());
            
// Testing for extra directives. See bug 94230.
var xx = <?xml version="1.0" encoding="UTF-8"?>
<?mso-infoPathSolution solutionVersion="1.0.0.26" productVersion="11.0.6250" PIVersion="1.0.0.0" href="file:///C:\Documents%20and%20Settings\Bob\BoB\Goodbye%20Doubt\Repository\CMS\Forms\PatternForm.xsn" name="urn:schemas-microsoft-com:office:infopath:PatternForm:urn-axiology-PatternDocument" language="en-us" ?>
<?mso-application progid="InfoPath.Document"?>;

AddTestCase("Testing for extra directives", "", xx.toString());

xx = new XML("<?xml version=\"1.0\" encoding=\"UTF-8\"?><?mso-infoPathSolution solutionVersion=\"1.0.0.26\" productVersion=\"11.0.6250\" PIVersion=\"1.0.0.0\" href=\"file:///C:\Documents%20and%20Settings\Bob\BoB\Goodbye%20Doubt\Repository\CMS\Forms\PatternForm.xsn\" name=\"urn:schemas-microsoft-com:office:infopath:PatternForm:urn-axiology-PatternDocument\" language=\"en-us\" ?><?mso-application progid=\"InfoPath.Document\"?>");

AddTestCase("Testing for extra directives", "", xx.toString());

END();
