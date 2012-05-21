/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER = 356247;
var summary = 'Decompilation of let {} = [1] in a loop';
var summarytrunk = 'let declaration must be direct child of block or top-level implicit block';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  var c;
  var f;
  var g;

  try
  { 
    c = '(function() { for(let x in []) let {} = [1]; })';
    f = eval(c);
    expect = 'function() { for(let x in []) let [] = [1]; }';
    actual = f + '';
    compareSource(expect, actual, summary + ': f : ' + c);

    g = eval('(' + f + ')');
    actual = g + '';
    compareSource(expect, actual, summary + ': g : ' + c);
  }
  catch(ex)
  {
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=408957
    expect = 'SyntaxError';
    actual = ex.name;
    reportCompare(expect, actual, summarytrunk + ': ' + c);
  }

  try
  {
    c = '(function() { while(0) let {} = [1]; })';
    f = eval(c);
    expect = 'function() { while(0) let [] = [1]; }';
    actual = f + '';
    compareSource(expect, actual, summary + ': f : ' + c);

    g = eval('(' + f + ')');
    actual = g + '';
    compareSource(expect, actual, summary + ': g : ' + c);
  }
  catch(ex)
  {
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=408957
    expect = 'SyntaxError';
    actual = ex.name;
    reportCompare(expect, actual, summarytrunk + ': ' + c);
  }

  exitFunc ('test');
}
