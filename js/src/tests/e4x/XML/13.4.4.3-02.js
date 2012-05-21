/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "13.4.4.3 - XML.appendChild should copy child";
var BUGNUMBER = 312692;
var actual = '';
var expect = 'error';

printBugNumber(BUGNUMBER);
START(summary);

try
{
    var node = <node/>;
    node.appendChild(node);

    var result = String(node);
    actual = 'no error';
}
catch(e)
{
    actual = 'error';
}
TEST(1, expect, actual);

END();
