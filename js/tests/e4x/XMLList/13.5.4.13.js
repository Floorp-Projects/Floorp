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

START("13.5.4.13 - XMLList hasSimpleContent()");

TEST(1, true, XMLList.prototype.hasOwnProperty("hasSimpleContent"));

// One element should be same as XML case
x = 
<>
<alpha attr1="value1">
    <bravo>one</bravo>
    <charlie>
        two
        three
        <echo>four</echo>
    </charlie>
    <delta />
    <foxtrot attr2="value2">five</foxtrot>
    <golf attr3="value3" />
    <hotel>
        six
        seven
    </hotel>
    <india><juliet /></india>
</alpha>;
</>;

TEST(2, false, x.hasSimpleContent());
TEST(3, true, x.bravo.hasSimpleContent());
TEST(4, false, x.charlie.hasSimpleContent());
TEST(5, true, x.delta.hasSimpleContent());
TEST(6, true, x.foxtrot.hasSimpleContent());
TEST(7, true, x.golf.hasSimpleContent());
TEST(8, true, x.hotel.hasSimpleContent());
TEST(9, true, x.@attr1.hasSimpleContent());
TEST(10, true, x.bravo.child(0).hasSimpleContent());
TEST(11, false, x.india.hasSimpleContent());

// More than one element is complex if one or more things in the list are elements.
x = 
<>
<alpha>one</alpha>
<bravo>two</bravo>
</>;

TEST(12, false, x.hasSimpleContent());

x =
<root>
    one
    <alpha>one</alpha>
</root>;

TEST(13, false, x.*.hasSimpleContent());

x =
<root attr1="value1" attr2="value2">
    one
</root>;

TEST(14, true, x.@*.hasSimpleContent());

END();