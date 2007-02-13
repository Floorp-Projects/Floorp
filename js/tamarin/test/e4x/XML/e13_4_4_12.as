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

START("13.4.4.12 - XML descendants");

//TEST(1, true, XML.prototype.hasOwnProperty("descendants"));

x1 =
<alpha>
    <bravo>one</bravo>
    <charlie>
        two
        <bravo>three</bravo>
    </charlie>
</alpha>;

TEST(2, <bravo>three</bravo>, x1.charlie.descendants("bravo"));
TEST(3, new XMLList("<bravo>one</bravo><bravo>three</bravo>"), x1.descendants("bravo"));

// Test *
correct = new XMLList("<bravo>one</bravo>one<charlie>two<bravo>three</bravo></charlie>two<bravo>three</bravo>three");

XML.prettyPrinting = false;
TEST(4, correct, x1.descendants("*"));
TEST(5, correct, x1.descendants());
XML.prettyPrinting = true;

XML.prettyPrinting = false;

var xmlDoc = "<MLB><foo>bar</foo><Team>Giants<foo>bar</foo></Team><City><foo>bar</foo>San Francisco</City><Team>Padres</Team><City>San Diego</City></MLB>";

AddTestCase( "MYXML = new XML(xmlDoc), MYXML.descendants('Team')", "<Team>Giants<foo>bar</foo></Team>" + NL() + "<Team>Padres</Team>", 
             (MYXML = new XML(xmlDoc), MYXML.descendants('Team').toString()) );
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.descendants('Team').length()", 2, 
             (MYXML = new XML(xmlDoc), MYXML.descendants('Team').length()) );
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.descendants('Team') instanceof XMLList", true, 
             (MYXML = new XML(xmlDoc), MYXML.descendants('Team') instanceof XMLList) );

// find multiple levels of descendants
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.descendants('foo')", "<foo>bar</foo>" + NL() + "<foo>bar</foo>" + NL() + "<foo>bar</foo>", 
             (MYXML = new XML(xmlDoc), MYXML.descendants('foo').toString()) );
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.descendants('foo').length()", 3, 
             (MYXML = new XML(xmlDoc), MYXML.descendants('foo').length()) );
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.descendants('foo') instanceof XMLList", true, 
             (MYXML = new XML(xmlDoc), MYXML.descendants('foo') instanceof XMLList) );

// no matching descendants
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.descendants('nomatch')", "", 
             (MYXML = new XML(xmlDoc), MYXML.descendants('nomatch').toString()) );
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.descendants('nomatch').length()", 0, 
             (MYXML = new XML(xmlDoc), MYXML.descendants('nomatch').length()) );
AddTestCase( "MYXML = new XML(xmlDoc), MYXML.descendants('nomatch') instanceof XMLList", true, 
             (MYXML = new XML(xmlDoc), MYXML.descendants('nomatch') instanceof XMLList) );
             
// descendant with hyphen

e = 
<employees>
    <employee id="1"><first-name>Joe</first-name><age>20</age></employee>
    <employee id="2"><first-name>Sue</first-name><age>30</age></employee>
</employees> 

correct =
<first-name>Joe</first-name> +
<first-name>Sue</first-name>;

names = e.descendants("first-name");

AddTestCase("Descendant with hyphen", correct, names);

END();
