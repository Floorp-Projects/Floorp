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

START("13.4.4.23 - XML namespace()");

TEST(1, true, XML.prototype.hasOwnProperty("namespace"));

// Prefix case
x =
<foo:alpha xmlns:foo="http://foo/" xmlns:bar="http://bar/">
    <foo:bravo>one</foo:bravo>
</foo:alpha>;

y = x.namespace();
TEST(2, "object", typeof(y));
TEST(3, Namespace("http://foo/"), y);

y = x.namespace("bar");
TEST(4, "object", typeof(y));
TEST(5, Namespace("http://bar/"), y);

// No Prefix Case
x =
<alpha xmlns="http://foobar/">
    <bravo>one</bravo>
</alpha>;

y = x.namespace();
TEST(6, "object", typeof(y));
TEST(7, Namespace("http://foobar/"), y);

// No Namespace case
x =
<alpha>
    <bravo>one</bravo>
</alpha>;
TEST(8, Namespace(""), x.namespace());

// Namespaces of attributes
x =
<alpha xmlns="http://foo/">
    <ns:bravo xmlns:ns="http://bar" name="two" ns:value="three">one</ns:bravo>
</alpha>;

var ns = new Namespace("http://bar");
y = x.ns::bravo.@name.namespace();
TEST(9, Namespace(""), y);

y = x.ns::bravo.@ns::value.namespace();
TEST(10, ns.toString(), y.toString());

END();