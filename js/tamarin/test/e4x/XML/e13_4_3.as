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

START("13.4.3 - XML Properties");

// Test defaults
TEST(1, true, XML.ignoreComments);    
TEST(2, true, XML.ignoreProcessingInstructions);    
TEST(3, true, XML.ignoreWhitespace);    
TEST(4, true, XML.prettyPrinting);    
TEST(5, 2, XML.prettyIndent);

// ignoreComments
x1 = <alpha><!-- comment --><bravo>one</bravo></alpha>;

correct = <alpha><bravo>one</bravo></alpha>;

TEST(6, correct, x1);

XML.ignoreComments = false;

x1 = <alpha><!-- comment --><bravo>one</bravo></alpha>;

correct = 
"<alpha>" + NL() +   
"  <!-- comment -->" + NL() +
"  <bravo>one</bravo>" + NL() +
"</alpha>";

TEST_XML(7, correct, x1);


// ignoreProcessingInstructions
XML.defaultSettings();
x1 = 
<>
    <alpha>
        <?foo version="1.0" encoding="utf-8"?>
        <bravo>one</bravo>
    </alpha>
</>;

correct = 
<alpha>
    <bravo>one</bravo>
</alpha>;

TEST(8, correct, x1);

XML.ignoreProcessingInstructions = false;

x1 = 
<>
    <alpha>
        <?foo version="1.0" encoding="utf-8"?>
        <bravo>one</bravo>
    </alpha>
</>;

correct = 
"<alpha>" + NL() +   
"  <?foo version=\"1.0\" encoding=\"utf-8\"?>" + NL() +
"  <bravo>one</bravo>" + NL() +
"</alpha>";

TEST_XML(9, correct, x1);

// ignoreWhitespace
XML.defaultSettings();
x1 = new XML("<alpha> \t\r\n\r\n<bravo> \t\r\n\r\none</bravo> \t\r\n\r\n</alpha>");

correct = 
"<alpha>" + NL() + 
"  <bravo>one</bravo>" + NL() +
"</alpha>";

TEST_XML(10, correct, x1);

XML.ignoreWhitespace = false;
XML.prettyPrinting = false;

correct = "<alpha> \n\n<bravo> \n\none</bravo> \n\n</alpha>";
x1 = new XML(correct);

TEST_XML(11, correct, x1);

// prettyPrinting
XML.prettyPrinting = true;

x1 =<alpha>one<bravo>two</bravo><charlie/><delta>three<echo>four</echo></delta></alpha>;

correct = "<alpha>" + NL() +
    "  one" + NL() +
    "  <bravo>two</bravo>" + NL() +
    "  <charlie/>" + NL() + 
    "  <delta>" + NL() +
    "    three" + NL() +
    "    <echo>four</echo>" + NL() +
    "  </delta>" + NL() +
    "</alpha>";
    
TEST(12, correct, x1.toString());
TEST(13, correct, x1.toXMLString());

XML.prettyPrinting = false;

correct = "<alpha>one<bravo>two</bravo><charlie/><delta>three<echo>four</echo></delta></alpha>";
TEST(14, correct, x1.toString());
TEST(15, correct, x1.toXMLString());

// prettyIndent
XML.prettyPrinting = true;
XML.prettyIndent = 3;

correct = "<alpha>" + NL() +
    "   one" + NL() +
    "   <bravo>two</bravo>" + NL() +
    "   <charlie/>" + NL() + 
    "   <delta>" + NL() +
    "      three" + NL() +
    "      <echo>four</echo>" + NL() +
    "   </delta>" + NL() +
    "</alpha>";

TEST(16, correct, x1.toString());
TEST(17, correct, x1.toXMLString());

XML.prettyIndent = 0;

correct = "<alpha>" + NL() +
    "one" + NL() +
    "<bravo>two</bravo>" + NL() +
    "<charlie/>" + NL() + 
    "<delta>" + NL() +
    "three" + NL() +
    "<echo>four</echo>" + NL() +
    "</delta>" + NL() +
    "</alpha>";

TEST(18, correct, x1.toString());
TEST(19, correct, x1.toXMLString());

// settings()
XML.defaultSettings();
o = XML.settings();
TEST(20, false, o.ignoreComments);
TEST(21, false, o.ignoreProcessingInstructions);
TEST(22, false, o.ignoreWhitespace);
TEST(23, true, o.prettyPrinting);
TEST(24, 0, o.prettyIndent);

// setSettings()
o = XML.settings();
o.ignoreComments = false;
o.ignoreProcessingInstructions = false;
o.ignoreWhitespace = false;
o.prettyPrinting = false;
o.prettyIndent = 7;

XML.setSettings(o);
o = XML.settings();
TEST(25, false, o.ignoreComments);
TEST(26, false, o.ignoreProcessingInstructions);
TEST(27, false, o.ignoreWhitespace);
TEST(28, false, o.prettyPrinting);
TEST(29, 7, o.prettyIndent);

// defaultSettings()
XML.defaultSettings();
o = XML.settings();
TEST(30, false, o.ignoreComments);
TEST(31, false, o.ignoreProcessingInstructions);
TEST(32, false, o.ignoreWhitespace);
TEST(33, false, o.prettyPrinting);
TEST(34, 7, o.prettyIndent);

END();
