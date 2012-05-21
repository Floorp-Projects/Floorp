/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


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
"<alpha>\n" +  
"  <!-- comment -->\n" +
"  <bravo>one</bravo>\n" +
"</alpha>";

TEST_XML(7, correct, x);


// ignoreProcessingInstructions
XML.setSettings();
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
"<alpha>\n" +  
"  <?foo version=\"1.0\" encoding=\"utf-8\"?>\n" +
"  <bravo>one</bravo>\n" +
"</alpha>";

TEST_XML(9, correct, x);

// ignoreWhitespace
XML.setSettings();
x = new XML("<alpha> \t\r\n\r\n<bravo> \t\r\n\r\none</bravo> \t\r\n\r\n</alpha>");

correct =
"<alpha>\n" +
"  <bravo>one</bravo>\n" +
"</alpha>";

TEST_XML(10, correct, x);

XML.ignoreWhitespace = false;
XML.prettyPrinting = false;

correct = "<alpha> \n\n<bravo> \n\none</bravo> \n\n</alpha>";
x = new XML(correct);

TEST_XML(11, correct, x);

// prettyPrinting
XML.setSettings();

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

correct = "<alpha>\n" +
    "  one\n" +
    "  <bravo>two</bravo>\n" +
    "  <charlie/>\n" +
    "  <delta>\n" +
    "    three\n" +
    "    <echo>four</echo>\n" +
    "  </delta>\n" +
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

correct = "<alpha>\n" +
    "   one\n" +
    "   <bravo>two</bravo>\n" +
    "   <charlie/>\n" +
    "   <delta>\n" +
    "      three\n" +
    "      <echo>four</echo>\n" +
    "   </delta>\n" +
    "</alpha>";

TEST(16, correct, x.toString());
TEST(17, correct, x.toXMLString());

XML.prettyIndent = 0;

correct = "<alpha>\n" +
    "one\n" +
    "<bravo>two</bravo>\n" +
    "<charlie/>\n" +
    "<delta>\n" +
    "three\n" +
    "<echo>four</echo>\n" +
    "</delta>\n" +
    "</alpha>";

TEST(18, correct, x.toString());
TEST(19, correct, x.toXMLString());

// settings()
XML.setSettings();
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

// setSettings()
XML.setSettings();
o = XML.settings();
TEST(30, true, o.ignoreComments);
TEST(31, true, o.ignoreProcessingInstructions);
TEST(32, true, o.ignoreWhitespace);
TEST(33, true, o.prettyPrinting);
TEST(34, 2, o.prettyIndent);

correct = new XML('');

// ignoreComments
XML.ignoreComments = true;
x = <!-- comment -->;
TEST(35, correct, x);

// ignoreProcessingInstructions
XML.ignoreProcessingInstructions = true;
x = <?pi?>;
TEST(36, correct, x);

END();
