/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 463259;
var summary = 'Do not assert: VALUE_IS_FUNCTION(cx, fval)';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 

try
{
  (function(){
    eval("(function(){ for (var j=0;j<4;++j) if (j==3) undefined(); })();");
  })();
}
catch(ex)
{
}


reportCompare(expect, actual, summary);
