/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 354145;
var summary = 'Immutable XML';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);


var list = <><a/><b/></>

function a() {
   delete list[1];
   delete list[0];
   gc();
   for (var i = 0; i != 50000; ++i)
     var tmp = ""+i;    
  return true;
}

var list2 = list.(a());

print(uneval(list2))

TEST(1, expect, actual);

END();
