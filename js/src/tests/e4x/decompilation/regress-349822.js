/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 349822;
var summary = 'decompilation of x.@[2]';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

expect = 'function () {\n    return x.@[2];\n}';

try
{
    var f = eval('(function () { return x.@[2]; })');
    actual = f + '';
}
catch(ex)
{
    actual = ex + '';
}

compareSource(expect, actual, inSection(1) + summary);

END();
