// |reftest| skip-if(Android)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 440558;
var summary = 'Do not assert: *flagp != GCF_FINAL';
var actual = 'No Crash';
var expect = 'No Crash';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  print('Note: this test requires that javascript.options.gczeal ' +
        'be set to 2 prior to the browser start');

  m = function(a, b) {
    if (++i < 10) {
    }
  };
  e = function(a, b) {
  };

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}

