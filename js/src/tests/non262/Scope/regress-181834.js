/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    25 November 2002
 * SUMMARY: Testing scope
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=181834
 *
 * This bug only bit in Rhino interpreted mode, when the
 * 'compile functions with dynamic scope' feature was set.
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 181834;
var summary = 'Testing scope';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


/*
 * If N<=0, |outer_d| just gets incremented once,
 * so the return value should be 1 in this case.
 *
 * If N>0, we end up calling inner() N+1 times:
 * inner(N), inner(N-1), ... , inner(0).
 *
 * Each call to inner() increments |outer_d| by 1.
 * The last call, inner(0), returns the final value
 * of |outer_d|, which should be N+1.
 */
function outer(N)
{
  var outer_d = 0;
  return inner(N);

  function inner(level)
  {
    outer_d++;

    if (level > 0)
      return inner(level - 1);
    else
      return outer_d;
  }
}


/*
 * This only has meaning in Rhino -
 */
setDynamicScope(true);

/*
 * Recompile the function |outer| via eval() in order to
 * feel the effect of the dynamic scope mode we have set.
 */
var s = outer.toString();
eval(s);

status = inSection(1);
actual = outer(-5);
expect = 1;
addThis();

status = inSection(2);
actual = outer(0);
expect = 1;
addThis();

status = inSection(3);
actual = outer(5);
expect = 6;
addThis();


/*
 * Sanity check: do same steps with the dynamic flag off
 */
setDynamicScope(false);

/*
 * Recompile the function |outer| via eval() in order to
 * feel the effect of the dynamic scope mode we have set.
 */
eval(s);

status = inSection(4);
actual = outer(-5);
expect = 1;
addThis();

status = inSection(5);
actual = outer(0);
expect = 1;
addThis();

status = inSection(6);
actual = outer(5);
expect = 6;
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function setDynamicScope(flag)
{
  if (this.Packages)
  {
    var cx = this.Packages.org.mozilla.javascript.Context.getCurrentContext();
    cx.setCompileFunctionsWithDynamicScope(flag);
  }
}


function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}


function test()
{
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }
}
