// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 347155;
var summary = 'Do not crash with deeply nested e4x literal';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

function repeat(str, num)
{
  var s="", i;
  for (i=0; i<num; ++i)
    s += str;
  return s;
}

n = 100000;
e4x = repeat("<x>", n) + 3 + repeat("</x>", n);
try
{
    eval(e4x);
}
catch(ex)
{
}

TEST(1, expect, actual);

END();
