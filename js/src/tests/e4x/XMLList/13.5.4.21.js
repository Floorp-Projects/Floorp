/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("13.5.4.21 - XMLList toXMLString()");

TEST(1, true, XMLList.prototype.hasOwnProperty("toXMLString"));
   
x = <><alpha>one</alpha></>;

TEST(2, "<alpha>one</alpha>", x.toXMLString());

x = <><alpha>one</alpha><bravo>two</bravo></>;

TEST(3, "<alpha>one</alpha>\n<bravo>two</bravo>", x.toXMLString());

END();
