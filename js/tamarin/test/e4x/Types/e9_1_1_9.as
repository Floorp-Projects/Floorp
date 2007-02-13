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

START("9.1.1.9 - XML [[Equals]]");

x1 = <alpha>one</alpha>;
y1 = <alpha>one</alpha>;
TEST(1, true, (x1 == y1) && (y1 == x1));

// Should return false if comparison is not XML
y1 = "<alpha>one</alpha>";
TEST(2, false, (x1 == y1) || (y1 == x1));

y1 = undefined
TEST(3, false, (x1 == y1) || (y1 == x1));

y1 = null
TEST(4, false, (x1 == y1) || (y1 == x1));

y1 = new Object();
TEST(5, false, (x1 == y1) || (y1 == x1));

// Check with attributes
x1 = <alpha attr1="value1">one<bravo attr2="value2">two</bravo></alpha>;
y1 = <alpha attr1="value1">one<bravo attr2="value2">two</bravo></alpha>;
TEST(6, true, (x1 == y1) && (y1 == x1));

y1 = <alpha attr1="new value">one<bravo attr2="value2">two</bravo></alpha>;
TEST(7, false, (x1 == y1) || (y1 == x1));

// Logical equality
// Attribute order.
x1 = <alpha attr1="value1" attr2="value2">one<bravo attr3="value3" attr4="value4">two</bravo></alpha>;
y1 = <alpha attr2="value2" attr1="value1">one<bravo attr4="value4" attr3="value3">two</bravo></alpha>;
TEST(8, true, (x1 == y1) && (y1 == x1));

// Skips empty text nodes
x1 = <alpha> <bravo>one</bravo> </alpha>;
y1 = <alpha><bravo>one</bravo></alpha>;
TEST(9, true, (x1 == y1) && (y1 == x1));


XML.ignoreWhitespace = false;

// Doesn't trim text nodes.
x1 = <alpha><bravo> one </bravo></alpha>;
y1 = <alpha><bravo>one</bravo></alpha>;
TEST(10, false, (x1 == y1) || (y1 == x1));

// Compare comments
XML.ignoreComments = false;
x1 = new XML('<alpha><!-- comment1 --><bravo><!-- comment2 -->one</bravo></alpha>');
y1 = new XML('<alpha><!-- comment2 --><bravo><!-- comment1 -->one</bravo></alpha>');
TEST(11, false, (x1 == y1) || (y1 == x1));

one = x1.*[0];
two = y1.*[0];
TEST(12, false, (one == two) || (two == one));

one = x1.*[0];
two = y1.bravo.*[0];
TEST(13, true, (one == two) && (two == one));

 
// Compare processing instructions
XML.ignoreProcessingInstructions = false;
x1 = new XML('<alpha><?one foo="bar" ?><bravo><?two bar="foo" ?>one</bravo></alpha>');
y1 = new XML('<alpha><?two bar="foo" ?><bravo><?one foo="bar" ?>one</bravo></alpha>');
TEST(14, false, (x1 == y1) || (y1 == x1));

one = x1.*[0];
two = y1.*[0];
TEST(15, false, (one == two) || (two == one));

one = x1.*[0];
two = y1.bravo.*[0];
TEST(16, true, (one == two) && (two == one));

// Namespaces
x1 = <ns1:alpha xmlns:ns1="http://foo/"><ns1:bravo>one</ns1:bravo></ns1:alpha>;
y1 = <ns2:alpha xmlns:ns2="http://foo/"><ns2:bravo>one</ns2:bravo></ns2:alpha>;
 
TEST(17, true, (x1 == y1) && (y1 == x1));

y1 = <ns2:alpha xmlns:ns2="http://foo"><ns2:bravo>one</ns2:bravo></ns2:alpha>;
TEST(18, false, (x1 == y1) || (y1 == x1));


// Default namespace
default xml namespace = "http://foo/";
x1 = <alpha xmlns="http://foo/">one</alpha>;
y1 = <alpha>one</alpha>;

TEST(19, true, (x1 == y1) && (y1 == x1));


END();
