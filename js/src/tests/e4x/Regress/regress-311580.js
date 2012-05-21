/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "Regression - properly root stack in toXMLString";
var BUGNUMBER = 311580;
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

// Not a performance problem.
var xmlOl = new XML('<ol><li>Item 1<\/li><li>Item 2<\/li><\/ol>');
var list =  xmlOl.li;

for(i = 0; i < 30000; i++)
{
    list[i+2] = "Item " + (i+3); // This code is slow.
}

var s = xmlOl.toXMLString();

TEST(1, expect, actual);

END();
