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

START("9.2.1.2 - XMLList [[Put]]");


// element

var x1 = new XMLList("<alpha>one</alpha><bravo>two</bravo>");

TEST(1, "<alpha>one</alpha>" + NL() + "<bravo>two</bravo>", 
  x1.toXMLString());

x1[0] = <charlie>three</charlie>;
TEST(2, "<charlie>three</charlie>" + NL() + "<bravo>two</bravo>",
  x1.toXMLString());

x1[0] = <delta>four</delta> + <echo>five</echo>;
TEST(3, "<delta>four</delta>" + NL() + "<echo>five</echo>" + NL() + "<bravo>two</bravo>", 
  x1.toXMLString());
  
var y1 = new XMLList("<alpha>one</alpha><bravo>two</bravo>");
y1[0] = "five";

TEST(4, "<alpha>five</alpha>" + NL() + "<bravo>two</bravo>", 
  y1.toXMLString());
  


// attribute

var x1 = new XMLList("<alpha attr=\"50\">one</alpha><bravo>two</bravo>");
x1[0].@attr = "fifty";
TEST(5, "<alpha attr=\"fifty\">one</alpha>" + NL() + "<bravo>two</bravo>", x1.toXMLString());

var x1 = new XMLList("<alpha attr=\"50\">one</alpha><bravo>two</bravo>");
x1[0].@attr = new XMLList("<att>sixty</att>");
TEST(6, "<alpha attr=\"sixty\">one</alpha>" + NL() + "<bravo>two</bravo>", x1.toXMLString());

var x1 = new XMLList("<alpha attr=\"50\">one</alpha><bravo>two</bravo>");
x1[0].@attr = "<att>sixty</att>";
TEST(7, "<alpha attr=\"&lt;att>sixty&lt;/att>\">one</alpha>" + NL() + "<bravo>two</bravo>", x1.toXMLString());


// text node

var x1 = new XMLList("alpha<bravo>two</bravo>");
x1[0] = "beta";
TEST(8, "beta" + NL() + "<bravo>two</bravo>", x1.toXMLString());

var x1 = new XMLList("<alpha>one</alpha><bravo>two</bravo>");
x1[0] = new XML("beta");
TEST(9, "<alpha>beta</alpha>" + NL() + "<bravo>two</bravo>", x1.toXMLString());

var x1 = new XMLList("<alpha>one</alpha><bravo>two</bravo>");
x1[0] = new XMLList("beta");
TEST(10, "beta" + NL() + "<bravo>two</bravo>", x1.toXMLString());

var x1 = new XMLList("alpha<bravo>two</bravo>");
x1[0] = new XML("<two>beta</two>");
TEST(11, "<two>beta</two>" + NL() + "<bravo>two</bravo>", x1.toXMLString());


// comment

var x1 = new XMLList("<alpha><beta><!-- hello, comment --></beta></alpha>");
x1.beta = new XMLList("<comment>hello</comment>");
TEST(12, new XMLList("<alpha><comment>hello</comment></alpha>"), x1);

var x1 = new XMLList("<alpha><beta><!-- hello, comment --></beta></alpha>");
x1.beta = new XML("<comment>hello</comment>");
TEST(13, new XMLList("<alpha><comment>hello</comment></alpha>"), x1);

var x1 = new XMLList("<alpha><beta><!-- hello, comment --></beta></alpha>");
x1.beta = "hello";
TEST(14, new XMLList("<alpha><beta>hello</beta></alpha>"), x1);

var x1 = new XMLList("<alpha><beta><!-- hello, comment --></beta></alpha>");
x1.beta = new XML("hello");
TEST(15, new XMLList("<alpha><beta>hello</beta></alpha>"), x1);


// PI

XML.ignoreProcessingInstructions = false;
var x1 = new XML("<alpha><beta><?xm-xsl-param name=\"sponsor\" value=\"dw\"?></beta></alpha>");
x1.beta = new XML("<pi element=\"yes\">instructions</pi>");
TEST(16, new XMLList("<alpha><pi element=\"yes\">instructions</pi></alpha>"), x1);

XML.ignoreProcessingInstructions = false;
var x1 = new XML("<alpha><beta><?xm-xsl-param name=\"sponsor\" value=\"dw\"?></beta></alpha>");
x1.beta = new XMLList("<pi element=\"yes\">instructions</pi>");
TEST(17, new XMLList("<alpha><pi element=\"yes\">instructions</pi></alpha>"), x1);

var x1 = new XML("<alpha><beta><?xm-xsl-param name=\"sponsor\" value=\"dw\"?></beta></alpha>");
x1.beta = "processing instructions";
TEST(18, new XMLList("<alpha><beta>processing instructions</beta></alpha>"), x1);

END();
