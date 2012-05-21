/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 188206;
var summary = 'Invalid use of regexp quantifiers should generate SyntaxErrors';
var CHECK_PASSED = 'Should not generate an error';
var CHECK_FAILED = 'Generated an error!';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


/*
 * Misusing the {DecmalDigits} quantifier - according to ECMA,
 * but not according to Perl.
 *
 * ECMA-262 Edition 3 prohibits the use of unescaped braces in
 * regexp patterns, unless they form part of a quantifier.
 *
 * Hovever, Perl does not prohibit this. If not used as part
 * of a quantifer, Perl treats braces literally.
 *
 * We decided to follow Perl on this for backward compatibility.
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=190685.
 *
 * Therefore NONE of the following ECMA violations should generate
 * a SyntaxError. Note we use checkThis() instead of testThis().
 */
status = inSection(13);
checkThis(' /a*{/ ');

status = inSection(14);
checkThis(' /a{}/ ');

status = inSection(15);
checkThis(' /{a/ ');

status = inSection(16);
checkThis(' /}a/ ');

status = inSection(17);
checkThis(' /x{abc}/ ');

status = inSection(18);
checkThis(' /{{0}/ ');

status = inSection(19);
checkThis(' /{{1}/ ');

status = inSection(20);
checkThis(' /x{{0}/ ');

status = inSection(21);
checkThis(' /x{{1}/ ');

status = inSection(22);
checkThis(' /x{{0}}/ ');

status = inSection(23);
checkThis(' /x{{1}}/ ');

status = inSection(24);
checkThis(' /x{{0}}/ ');

status = inSection(25);
checkThis(' /x{{1}}/ ');

status = inSection(26);
checkThis(' /x{{0}}/ ');

status = inSection(27);
checkThis(' /x{{1}}/ ');


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



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
