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

START("13.4 XML Object - Miscellaneous errors");

function grabError(err, str) {
	var typeIndex = str.indexOf("Error:");
	var type = str.substr(0, typeIndex + 5);
	if (type == "TypeError") {
		AddTestCase("Asserting for TypeError", true, (err is TypeError));
	}
	var numIndex = str.indexOf("Error #");
	var num = str.substr(numIndex, 11);
	return num;
}

var expected, result;

// missing quote after third

expectedStr = "TypeError: Error #1095: XML parser failure: Unterminated attribute";
expected = "Error #1095";

try{

var x1 = new XML("<song which=\"third><cd>Blue Train</cd><title>Locomotion</title><artist>John Coltrane</artist></song>");
throw "missing \"";

} catch( e1 ){

result = grabError(e1, e1.toString());

}
 
AddTestCase( "missing \" after attribute", expected, result );


// missing quotes around third

expectedStr = "TypeError: Error #1090: XML parser failure: element is malformed";
expected = "Error #1090";

try{

x1 = new XML("<song which=third><cd>Blue Train</cd><title>Locomotion</title><artist>John Coltrane</artist></song>");
throw "missing quotes around attribute";

} catch( e2 ){

result = grabError(e2, e2.toString());

}
 
AddTestCase( "missing quotes around attribute", expected, result );


// missing starting quote around third

expectedStr = "TypeError: Error #1090: XML parser failure: element is malformed";
expected = "Error #1090";

try{

x1 = new XML("<song which=third\"><cd>Blue Train</cd><title>Locomotion</title><artist>John Coltrane</artist></song>");
throw "missing starting quote for attribute";

} catch( e3 ){

result = grabError(e3, e3.toString());

}
 
AddTestCase( "missing starting quote for attribute", expected, result );



// missing ! at start of CDATA section

expectedStr = "TypeError: Error #1090: XML parser failure: element is malformed";
expected = "Error #1090";

try{

x1 = new XML("<songlist> <[CDATA[    <?xml version='1.0'?>   <entry>           <name>John Doe</name>         <email href='mailto:jdoe@somewhere.com'/> </entry>]]>       <![CDATA[   count = 0; function incCount() {   count++;    }]]></songlist>");
throw "missing ! at start of CDATA section";

} catch( e4 ){

result = grabError(e4, e4.toString());

}
 
AddTestCase( "missing ! at start of CDATA section", expected, result );


// unterminated CDATA

expectedStr = "TypeError: Error #1091: XML parser failure: Unterminated CDATA section";
expected = "Error #1091";

try{

x1 = new XML("<songlist> <![CDATA[   <?xml version='1.0'?>   <entry>           <name>John Doe</name>         <email href='mailto:jdoe@somewhere.com'/> </entry>]>       <![CDATA[   count = 0; function incCount() {   count++;    }]></songlist>");
throw "unterminated CDATA section";

} catch( e5 ){

result = grabError(e5, e5.toString());

}
 
AddTestCase( "unterminated CDATA section", expected, result );


// unterminated comment

expectedStr = "TypeError: Error #1094: XML parser failure: Unterminated comment";
expected = "Error #1094";

try{

x1 = new XML("<!-- Alex Homer's Book List with Linked Schema");
throw "unterminated comment";

} catch( e6 ){

result = grabError(e6, e6.toString());

}
 
AddTestCase( "unterminated comment", expected, result );


// unterminated doctype 

expectedStr = "TypeError: Error #1093: XML parser failure: Unterminated DOCTYPE declaration";
expected = "Error #1093";

try{

x1 = new XML("<!DOCTYPE PUBLIST [");
throw "unterminated doctype";

} catch( e7 ){

result = grabError(e7, e7.toString());

}
 
AddTestCase( "unterminated doctype", expected, result );


// bad attribute (E4X returns malformed element)

expectedStr = "TypeError: Error #1090: XML parser failure: element is malformed";
expected = "Error #1090";

try{

x1 = new XML("<song which='third'><cd>Blue Train</cd>  <title Locomotion</title> <artist>John Coltrane</artist></song>");
throw "bad attribute";

} catch( e8 ){

result = grabError(e8, e8.toString());

}
 
AddTestCase( "bad attribute", expected, result );


// "cd" must be terminated by "/cd"

expectedStr = "TypeError: Error #1085: The element type \"cd\" must be terminated by the matching end-tag \"</cd>\".";
expected = "Error #1085";

try{

x1 = new XML("<song which='third'><cd>Blue Train<title>Locomotion</title> <artist>John Coltrane</artist></song>");
throw "unterminated tag";

} catch( e9 ){

result = grabError(e9, e9.toString());

}
 
AddTestCase( "unterminated tag", expected, result );


// "song" must be terminated by "/song"

expectedStr = "TypeError: Error #1085: The element type \"song\" must be terminated by the matching end-tag \"</song>\".";
expected = "Error #1085";

try{

x1 = new XML("<song which='third'><cd>Blue Train</cd></title><artist>John Coltrane</artist></song>");
throw "mismatched end tag";

} catch( e10 ){

result = grabError(e10, e10.toString());

}
 
AddTestCase( "mismatched end tag", expected, result );


// "song" must be terminated by "/song"

expectedStr = "TypeError: Error #1085: The element type \"song\" must be terminated by the matching end-tag \"</song>\".";
expected = "Error #1085";

try{

x1 = new XML("<song which='third'><cd>Blue Train</cd></title><artist>John Coltrane</artist></SONG>");
throw "wrong case in root end tag";

} catch( e11 ){

result = grabError(e11, e11.toString());

}
 
AddTestCase( "wrong case in root end tag", expected, result );


// "cd" must be terminated by "/cd"

expectedStr = "TypeError: Error #1085: The element type \"cd\" must be terminated by the matching end-tag \"</cd>\".";
expected = "Error #1085";

try{

x1 = new XML("<song which='third'><cd>Blue Train</CD></title><artist>John Coltrane</artist></song>");
throw "wrong case in end tag";

} catch( e12 ){

result = grabError(e12, e12.toString());

}
 
AddTestCase( "wrong case end tag", expected, result );


// Rhino: Attribute name "name" associated with an element type "cd" must be followed by the "=" character

// E4X: element is malformed

expectedStr = "TypeError: Error #1090: XML parser failure: element is malformed";
expected = "Error #1090";

try{

x1 = new XML("<song which='third'><cd name>Blue Train</cd></title><artist>John Coltrane</artist></SONG>");
throw "missing attribute value";

} catch( e13 ){

result = grabError(e13, e13.toString());

}
 
AddTestCase( "missing attribute value", expected, result );



// E4X: unterminated XML decl

expectedStr = "TypeError: Error #1092: XML parser failure: Unterminated XML declaration";
expected = "Error #1092";

try{

x1 = new XML("<?xml version='1.0' encoding='UTF-8' standalone='no'><foo></foo>");
throw "unterminated XML decl";

} catch( e14 ){

result = grabError(e14, e14.toString());

}
 
AddTestCase( "unterminated XML decl", expected, result );


// Rhino: XML document structures must start and end within the same entity:

// E4X: same error

expectedStr = "TypeError: Error #1088: The markup in the document following the root element must be well-formed.";
expected = "Error #1088";

try{

x1 = new XML("<a>foo</a><b>bar</b>");
throw "XML must start and end with same entity";

} catch( e15 ){

result = grabError(e15, e15.toString());

}
 
AddTestCase( "XML must start and end with same entity", expected, result );



// Rhino: XML document structures must start and end within the same entity:

// E4X: same error

var y1 = new XMLList ("<a>foo</a><b>bar</b>");

expectedStr = "TypeError: Error #1088: The markup in the document following the root element must be well-formed.";
expected = "Error #1088";

try{

x1 = new XML(y1);
throw "XML must start and end with same entity";

} catch( e16 ){

result = grabError(e16, e16.toString());

}
 
AddTestCase( "XML must start and end with same entity", expected, result );


END();
