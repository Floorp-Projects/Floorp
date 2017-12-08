/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 15 Feb 2001
 *
 * SUMMARY:  self.eval(str) inside a function
 * NOTE: 'self' is just a variable used to capture the global JS object.
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=68498
 * See http://bugzilla.mozilla.org/showattachment.cgi?attach_id=25251
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=69441 (!!!)
 *
 * Brendan:
 *
 * "ECMA-262 Edition 3, 10.1.3 requires a FunctionDeclaration parsed as part
 * of  a Program by eval to create a property of eval's caller's variable object.
 * This test evals in the body of a with statement, whose scope chain *is*
 * relevant to the effect of parsing the FunctionDeclaration."
 */
//-----------------------------------------------------------------------------
var BUGNUMBER = 68498;
var summary = 'Testing self.eval(str) inside a function';
var statprefix = '; currently at expect[';
var statsuffix = '] within test -';
var sToEval='';
var actual=[ ];
var expect=[ ];


// Capture a reference to the global object -
var self = this;

// You shouldn't see this global variable's value in any printout -
var x = 'outer';

// This function is the heart of the test -
function f(o,s,x) {with(o) eval(s); return z;};

// Run-time statements to pass to the eval inside f
sToEval += 'actual[0] = typeof g;'
sToEval += 'function g(){actual[1]=(typeof w == "undefined"  ||  w); return x};'
sToEval += 'actual[2] = w;'
sToEval += 'actual[3] = typeof g;'
sToEval += 'var z=g();'

// Set the actual-results array. The next line will set actual[0] - actual[4] in one shot
actual[4] = f({w:44}, sToEval, 'inner');
actual[5] = 'z' in self && z;


/* Set the expected-results array.
 *
 *  Sample issue: why do we set expect[4] = 'inner'?  Look at actual[4]...
 *  1. The return value of f equals z, which is not defined at compile-time
 *  2. At run-time (via with(o) eval(s) inside f), z is defined as the return value of g
 *  3. At run-time (via with(o) eval(s) inside f), g is defined to return x
 *  4. In the scope of with(o), x is undefined
 *  5. Farther up the scope chain, x can be located as an argument of f
 *  6. The value of this argument at run-time is 'inner'
 *  7. Even farther up the scope chain, the name x can be found as a global variable
 *  8. The value of this global variable is 'outer', but we should NOT have gone
 *      this far up the scope chain to find x...therefore we expect 'inner'
 */
expect[0] = 'function';
expect[1] = 44;
expect[2] = 44;
expect[3] = 'function';
expect[4] = 'inner';
expect[5] = false;



//------------------------------------------------------------------------------------------------
test();
//-------------------------------------------------------------------------------------------------


function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (var i in expect)
  {
    reportCompare(expect[i], actual[i], getStatus(i));
  }
}


function getStatus(i)
{
  return (summary  +  statprefix  +  i  +  statsuffix);
}
