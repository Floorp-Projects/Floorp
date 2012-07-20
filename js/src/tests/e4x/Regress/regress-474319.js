// |reftest| pref(javascript.options.xml.content,true) skip-if(Android)
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = 'Do not crash with e4x, map and concat';
var BUGNUMBER = 474319;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

if (typeof gczeal != 'function' || !('map' in Array.prototype))
{
    expect = actual = 'Test skipped due to lack of gczeal and Array.prototype.map';
}
else
{
    gczeal(2);
    try
    {
      (function(){[<y><z/></y>].map(''.concat)})();
    }
    catch(ex)
    {
    }
    gczeal(0);
}
TEST(1, expect, actual);

END();
