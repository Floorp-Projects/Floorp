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

START("13.4.3 - XML Properties");

// Test defaults
TEST(1, true, XML.ignoreComments);    
TEST(2, true, XML.ignoreProcessingInstructions);    
TEST(3, true, XML.ignoreWhitespace);    
TEST(4, true, XML.prettyPrinting);    
TEST(5, 2, XML.prettyIndent);

// ignoreComments
x = <alpha><!-- comment --><bravo>one</bravo></alpha>;

correct = <alpha><bravo>one</bravo></alpha>;

TEST(6, correct, x);

XML.ignoreComments = false;

x = <alpha><!-- comment --><bravo>one</bravo></alpha>;

correct = 
"<alpha>" + NL() +   
"  <!-- comment -->" + NL() +
"  <bravo>one</bravo>" + NL() +
"</alpha>";

TEST_XML(7, correct, x);


// ignoreProcessingInstructions
XML.defaultSettings();
x = 
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

TEST(8, correct, x);

XML.ignoreProcessingInstructions = false;

x = 
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

TEST_XML(9, correct, x);

// ignoreWhitespace
XML.defaultSettings();
x = new XML("<alpha> \t\r\n\r\n<bravo> \t\r\n\r\none</bravo> \t\r\n\r\n</alpha>");

correct = 
"<alpha>" + NL() + 
"  <bravo>one</bravo>" + NL() +
"</alpha>";

TEST_XML(10, correct, x);

XML.ignoreWhitespace = false;
XML.prettyPrinting = false;

correct = "<alpha> \n\n<bravo> \n\none</bravo> \n\n</alpha>";
x = new XML(correct);

TEST_XML(11, correct, x);

// prettyPrinting
XML.defaultSettings();

x =
<alpha>
    one
    <bravo>two</bravo>
    <charlie/>
    <delta>
      three
      <echo>four</echo>
    </delta>
</alpha>;

correct = "<alpha>" + NL() +
    "  one" + NL() +
    "  <bravo>two</bravo>" + NL() +
    "  <charlie/>" + NL() + 
    "  <delta>" + NL() +
    "    three" + NL() +
    "    <echo>four</echo>" + NL() +
    "  </delta>" + NL() +
    "</alpha>";
    
TEST(12, correct, x.toString());
TEST(13, correct, x.toXMLString());

XML.prettyPrinting = false;

correct = "<alpha>one<bravo>two</bravo><charlie/><delta>three<echo>four</echo></delta></alpha>";
TEST(14, correct, x.toString());
TEST(15, correct, x.toXMLString());

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

TEST(16, correct, x.toString());
TEST(17, correct, x.toXMLString());

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

TEST(18, correct, x.toString());
TEST(19, correct, x.toXMLString());

// settings()
XML.defaultSettings();
o = XML.settings();
TEST(20, true, o.ignoreComments);
TEST(21, true, o.ignoreProcessingInstructions);
TEST(22, true, o.ignoreWhitespace);
TEST(23, true, o.prettyPrinting);
TEST(24, 2, o.prettyIndent);

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
TEST(30, true, o.ignoreComments);
TEST(31, true, o.ignoreProcessingInstructions);
TEST(32, true, o.ignoreWhitespace);
TEST(33, true, o.prettyPrinting);
TEST(34, 2, o.prettyIndent);

END();