// |reftest| skip
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 446169;
var summary = 'Do not assert: Thin_GetWait(tl->owner) in thread-safe build';
var actual = 'No Crash';
var expect = 'No Crash';

var array = [];
for (var i = 0; i != 100*1000; i += 2) {
    array[i] = i;
    array[i+1] = i;
}

var src = array.join(';x');

function f() {
    new Function(src);
}

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  if (typeof scatter == 'function')
  {
    scatter([f, f, f, f]);
  }
  else
  {
    print('Test skipped. Requires thread-safe build with scatter function.');
  }
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}


