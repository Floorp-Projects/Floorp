/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


START("Assignment to XML property removes attributes");
printBugNumber(369536);

var x = <foo><bar id="1">bazOne</bar><bar id="2">bazTwo</bar></foo>;
TEST_XML(1, "<bar id=\"2\">bazTwo</bar>", x.bar[1]);
x.bar[1] = "bazTwoChanged";
TEST_XML(2, "<bar id=\"2\">bazTwoChanged</bar>", x.bar[1]);
