/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = 'throw error when two attributes with the same local name and ' +
    'the same namespace';
var BUGNUMBER = 283972;
var actual = 'no error';
var expect = 'error';

printBugNumber(BUGNUMBER);
START(summary);

try
{
    var xml = <god xmlns:pf1="http://example.com/2005/02/pf1"
        xmlns:pf2="http://example.com/2005/02/pf1"
        pf1:name="Kibo"
        pf2:name="Xibo" />;
    printStatus(xml.toXMLString());
}
catch(e)
{
    actual = 'error';
}

TEST(1, expect, actual);

END();
