/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 07 May 2001
 *
 * SUMMARY: Testing the switch statement
 *
 * See ECMA3  Section 12.11,  "The switch Statement"
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = '(none)';
var summary = 'Testing the switch statement';
var cnMatch = 'Match';
var cnNoMatch = 'NoMatch';
var status = '';
var statusitems = [ ];
var actual = '';
var actualvalues = [ ];
var expect= '';
var expectedvalues = [ ];


status = 'Section A of test';
actual = match(17, f(fInverse(17)), f, fInverse);
expect = cnMatch;
addThis();

status = 'Section B of test';
actual = match(17, 18, f, fInverse);
expect = cnNoMatch;
addThis();

status = 'Section C of test';
actual = match(1, 1, Math.exp, Math.log);
expect = cnMatch;
addThis();

status = 'Section D of test';
actual = match(1, 2, Math.exp, Math.log);
expect = cnNoMatch;
addThis();

status = 'Section E of test';
actual = match(1, 1, Math.sin, Math.cos);
expect = cnNoMatch;
addThis();



//---------------------------------------------------------------------------------
test();
//---------------------------------------------------------------------------------



/*
 * If F,G are inverse functions and x==y, this should return cnMatch -
 */
function match(x, y, F, G)
{
  switch (x)
  {
  case F(G(y)):
    return cnMatch;

  default:
    return cnNoMatch;
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
  printStatus (summary);

  for (var i = 0; i < UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }
}


function f(m)
{
  return 2*(m+1);
}


function fInverse(n)
{
  return (n-2)/2;
}
