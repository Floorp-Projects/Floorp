/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    21 January 2003
 * SUMMARY: Invalid use of regexp quantifiers should generate SyntaxErrors
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=188206
 * and http://bugzilla.mozilla.org/show_bug.cgi?id=85721#c48 etc.
 * and http://bugzilla.mozilla.org/show_bug.cgi?id=190685
 * and http://bugzilla.mozilla.org/show_bug.cgi?id=197451
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 188206;
var summary = 'Invalid use of regexp quantifiers should generate SyntaxErrors';
var TEST_PASSED = 'SyntaxError';
var TEST_FAILED = 'Generated an error, but NOT a SyntaxError!';
var TEST_FAILED_BADLY = 'Did not generate ANY error!!!';
var CHECK_PASSED = 'Should not generate an error';
var CHECK_FAILED = 'Generated an error!';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


/*
 * All the following are invalid uses of regexp quantifiers and
 * should generate SyntaxErrors. That's what we're testing for.
 *
 * To allow the test to compile and run, we have to hide the errors
 * inside eval strings, and check they are caught at run-time -
 *
 */
status = inSection(1);
testThis(' /a**/ ');

status = inSection(2);
testThis(' /a***/ ');

status = inSection(3);
testThis(' /a++/ ');

status = inSection(4);
testThis(' /a+++/ ');

/*
 * The ? quantifier, unlike * or +, may appear twice in succession.
 * Thus we need at least three in a row to provoke a SyntaxError -
 */

status = inSection(5);
testThis(' /a???/ ');

status = inSection(6);
testThis(' /a????/ ');


/*
 * Now do some weird things on the left side of the regexps -
 */
status = inSection(9);
testThis(' /+a/ ');

status = inSection(10);
testThis(' /++a/ ');

status = inSection(11);
testThis(' /?a/ ');

status = inSection(12);
testThis(' /??a/ ');


/*
 * Misusing the {DecmalDigits} quantifier - according to BOTH ECMA and Perl.
 *
 * Just as with the * and + quantifiers above, can't have two {DecmalDigits}
 * quantifiers in succession - it's a SyntaxError.
 */
status = inSection(28);
testThis(' /x{1}{1}/ ');

status = inSection(29);
testThis(' /x{1,}{1}/ ');

status = inSection(30);
testThis(' /x{1,2}{1}/ ');

status = inSection(31);
testThis(' /x{1}{1,}/ ');

status = inSection(32);
testThis(' /x{1,}{1,}/ ');

status = inSection(33);
testThis(' /x{1,2}{1,}/ ');

status = inSection(34);
testThis(' /x{1}{1,2}/ ');

status = inSection(35);
testThis(' /x{1,}{1,2}/ ');

status = inSection(36);
testThis(' /x{1,2}{1,2}/ ');



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



/*
 * Invalid syntax should generate a SyntaxError
 */
function testThis(sInvalidSyntax)
{
  expect = TEST_PASSED;
  actual = TEST_FAILED_BADLY;

  try
  {
    eval(sInvalidSyntax);
  }
  catch(e)
  {
    if (e instanceof SyntaxError)
      actual = TEST_PASSED;
    else
      actual = TEST_FAILED;
  }

  statusitems[UBound] = status;
  expectedvalues[UBound] = expect;
  actualvalues[UBound] = actual;
  UBound++;
}


/*
 * Allowed syntax shouldn't generate any errors
 */
function checkThis(sAllowedSyntax)
{
  expect = CHECK_PASSED;
  actual = CHECK_PASSED;

  try
  {
    eval(sAllowedSyntax);
  }
  catch(e)
  {
    actual = CHECK_FAILED;
  }

  statusitems[UBound] = status;
  expectedvalues[UBound] = expect;
  actualvalues[UBound] = actual;
  UBound++;
}


function test()
{
  enterFunc('test');
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
