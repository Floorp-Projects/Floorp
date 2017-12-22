/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 476049;
var summary = 'JSOP_DEFVAR enables gvar optimization for non-permanent properties';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

// This test requires either two input files in the shell or two 
// script blocks in the browser. 

if (typeof window == 'undefined')
{
  print(expect = actual = 'Test skipped');
}
else
{
  document.write(
    '<script type="text/javascript">' +
    'for (var i = 0; i != 1000; ++i)' + 
    '  this["a"+i] = 0;' + 
    'eval("var x");' + 
    'for (var i = 0; i != 1000; ++i)' + 
    '  delete this["a"+i];' + 
    '<\/script>'
    );

  document.write(
    '<script type="text/javascript">' +
    'var x;' + 
    'eval("delete x;");' +
    'x={};' +
    '<\/script>'
    );
}

reportCompare(expect, actual, summary);
