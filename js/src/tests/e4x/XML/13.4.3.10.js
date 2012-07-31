// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.4.3.10 - XML Constructor [[HasInstance]]");

printBugNumber(288027);

var xmlListObject1 = new XMLList('<god>Kibo</god>');
var xmlListObject2 = new XMLList('<god>Kibo</god><devil>Xibo</devil>');

TEST(1, true, xmlListObject1 instanceof XML);
TEST(2, true, xmlListObject2 instanceof XML);

END();
