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

START("13.5.4.4 - XMLList child()");

TEST(1, true, XMLList.prototype.hasOwnProperty("child"));

// Test with XMLList of size 0
x = new XMLList()
TEST(2, "xml", typeof(x.child("bravo")));
TEST_XML(3, "", x.child("bravo"));

// Test with XMLList of size 1
x += <alpha>one<bravo>two</bravo></alpha>;    
TEST(4, "xml", typeof(x.child("bravo")));
TEST_XML(5, "<bravo>two</bravo>", x.child("bravo"));

x += <charlie><bravo>three</bravo></charlie>;
TEST(6, "xml", typeof(x.child("bravo")));

correct = <><bravo>two</bravo><bravo>three</bravo></>;
TEST(7, correct, x.child("bravo"));

// Test no match, null and undefined
TEST(8, "xml", typeof(x.child("foobar")));
TEST_XML(9, "", x.child("foobar"));

try {
  x.child(null);
  SHOULD_THROW(10);
} catch (ex) {
  TEST(10, "TypeError", ex.name);
}

// Test numeric inputs
x = 
<alpha>
    <bravo>one</bravo>
    <charlie>two</charlie>
</alpha>;

TEST(12, <bravo>one</bravo>, x.child(0));
TEST(13, <charlie>two</charlie>, x.child(1));

END();