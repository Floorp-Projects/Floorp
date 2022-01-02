// |reftest| skip-if(!this.uneval)

/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 254375;
var summary = 'Object.toSource for negative number property names';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

  try
  {
    expect = 'no error';
    eval(uneval({'-1':true}));
    actual = 'no error';
  }
  catch(e)
  {
    actual = 'error';
  }

  reportCompare(expect, actual, summary);
