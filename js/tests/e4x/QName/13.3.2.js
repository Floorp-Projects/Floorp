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

START("13.3.2 - QName Constructor");

q = new QName("*");
TEST(1, "object", typeof(q));
TEST(2, "*", q.localName);
TEST(3, null, q.uri);
TEST(4, "*::*", q.toString());

// Default namespace
q = new QName("foobar");
TEST(5, "object", typeof(q));
TEST(6, "foobar", q.localName);
TEST(7, "", q.uri);
TEST(8, "foobar", q.toString());

q = new QName("http://foobar/", "foobar");
TEST(9, "object", typeof(q));
TEST(10, "foobar", q.localName);
TEST(11, "http://foobar/", q.uri);
TEST(12, "http://foobar/::foobar", q.toString());

p = new QName(q);
TEST(13, typeof(p), typeof(q));
TEST(14, q.localName, p.localName);
TEST(15, q.uri, p.uri);

n = new Namespace("http://foobar/");
q = new QName(n, "foobar");
TEST(16, "object", typeof(q));

q = new QName(null);
TEST(17, "object", typeof(q));
TEST(18, "null", q.localName);
TEST(19, "", q.uri);
TEST(20, "null", q.toString());

q = new QName(null, null);
TEST(21, "object", typeof(q));
TEST(22, "null", q.localName);
TEST(23, null, q.uri);
TEST(24, "*::null", q.toString());

END();