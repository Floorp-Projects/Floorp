/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    29 April 2003
 * SUMMARY: eval() is not a constructor, but don't crash on |new eval();|
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=204210
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 204210;
var summary = "eval() is not a constructor, but don't crash on |new eval();|";
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];

printBugNumber(BUGNUMBER);
printStatus(summary);

/*
 * Just testing that we don't crash on any of these constructs -
 */


/*
 * global scope -
 */
try
{
  var x = new eval();
  new eval();
}
catch(e)
{
}


/*
 * function scope -
 */
f();
function f()
{
  try
  {
    var x = new eval();
    new eval();
  }
  catch(e)
  {
  }
}


/*
 * eval scope -
 */
var s = '';
s += 'try';
s += '{';
s += '  var x = new eval();';
s += '  new eval();';
s += '}';
s += 'catch(e)';
s += '{';
s += '}';
eval(s);


/*
 * some combinations of scope -
 */
s = '';
s += 'function g()';
s += '{';
s += '  try';
s += '  {';
s += '    var x = new eval();';
s += '    new eval();';
s += '  }';
s += '  catch(e)';
s += '  {';
s += '  }';
s += '}';
s += 'g();';
eval(s);


function h()
{
  var s = '';
  s += 'function f()';
  s += '{';
  s += '  try';
  s += '  {';
  s += '    var x = new eval();';
  s += '    new eval();';
  s += '  }';
  s += '  catch(e)';
  s += '  {';
  s += '  }';
  s += '}';
  s += 'f();';
  eval(s);
}
h();

reportCompare('No Crash', 'No Crash', '');
