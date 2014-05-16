/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 440926;
var summary = 'Correctly match regexps with special "i" characters';
var actual = '';
var expect0 = '#I#,#I#;#I#,#I#';
var expect1 = 'iI#,iI#;iI#,iI#';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  actual += 'iI\u0130'.replace(/[\u0130]/gi, '#');
  actual += ',' + 'iI\u0130'.replace(/\u0130/gi, '#');

  jit(true);
  actual += ';' + 'iI\u0130'.replace(/[\u0130]/gi, '#');
  actual += ',' + 'iI\u0130'.replace(/\u0130/gi, '#');
  jit(false);

  // The result we get depends on the regexp engine in use.
  if (actual == expect0)
    reportCompare(expect0, actual, summary);
  else
    reportCompare(expect1, actual, summary);

  exitFunc ('test');
}
