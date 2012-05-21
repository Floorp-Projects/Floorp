/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var summary = "Hang due to cycle in XML object";
var BUGNUMBER = 327564;

printBugNumber(BUGNUMBER);
START(summary);

var p = <p/>;

p.c = 1;

var c = p.c[0];

p.insertChildBefore(null,c);

printStatus(p.toXMLString());

printStatus('p.c[1] === c');
TEST(1, true, p.c[1] === c);

p.c = 2

try
{
    c.appendChild(p)
    // iloop here
}
catch(ex)
{
    actual = ex instanceof Error;
}

TEST(2, true, actual);

END();
