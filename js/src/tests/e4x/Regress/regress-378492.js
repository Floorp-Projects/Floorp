/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 378492;
var summary = 'namespace_trace/qname_trace should check for null private, ' +
    'WAY_TOO_MUCH_GC';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

try
{
    eval('<x>');
}
catch(ex)
{
}

TEST(1, expect, actual);

END();
