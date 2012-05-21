// |reftest| fails
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "Undeclaring namespace prefix should cause parse error";
var BUGNUMBER = 292863;
var actual = 'no error';
var expect = 'error';

printBugNumber(BUGNUMBER);
START(summary);

try
{
    var godList = <gods:gods xmlns:gods="http://example.com/2005/05/04/gods">
        <god xmlns:god="">Kibo</god>
        </gods:gods>;
    printStatus(godList.toXMLString());
}
catch(e)
{
    actual = 'error';
}

TEST(1, expect, actual);

END();
