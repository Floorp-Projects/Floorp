/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 476871;
var summary = 'Do not crash @ js_StepXMLListFilter';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

jit(true);

try
{
  this.watch("NaN", /a/g)
    let(x)
    ((function(){
        for each (NaN in [null, '', '', '', '']) true;
      }
      )());

  NaN.( /x/ );
}
catch(ex)
{
  print(ex + '');
}

jit(false);

reportCompare(expect, actual, summary);
