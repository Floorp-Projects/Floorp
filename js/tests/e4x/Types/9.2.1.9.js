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

START("9.2.1.9 XMLList [[Equals]]");

// Empty list should equal undefined
TEST(1, true, (new XMLList() == undefined) && (undefined == new XMLList()));

// Compare two lists if all are equal
x = <alpha>one</alpha> + <bravo>two</bravo>;
y = <alpha>one</alpha> + <bravo>two</bravo>;
TEST(2, true, (x == y) && (y == x));
y = <alpha>one</alpha> + <bravo>two</bravo> + <charlie>three</charlie>;
TEST(3, false, (x == y) || (y == x));
y = <alpha>one</alpha> + <bravo>not</bravo>;
TEST(4, false, (x == y) || (y == x));

// If XMLList has one argument should compare with just the 0th element.
x = new XMLList(<alpha>one</alpha>);
y = <alpha>one</alpha>;
TEST(5, true, (x == y) && (y == x));
y = "one";
TEST(6, true, (x == y) && (y == x));

// Should return false even if list contains element equal to comparison
x = <alpha>one</alpha> + <bravo>two</bravo>;
y = <alpha>one</alpha>;
TEST(7, false, (x == y) && (y == x));

y = "<alpha>one</alpha>";
TEST(8, false, (x == y) || (y == x));

// Try other types - should return false
y = null;
TEST(9, false, (x == y) || (y == x));

y = new Object();
TEST(10, false, (x == y) || (y == x));

END();