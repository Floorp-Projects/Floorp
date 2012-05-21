/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    23 October 2003
 * SUMMARY: Unescaped, unbalanced parens in a regexp should cause SyntaxError.
 *
 * The same would also be true for unescaped, unbalanced brackets or braces
 * if we followed the ECMA-262 Ed. 3 spec on this. But it was decided for
 * backward compatibility reasons to follow Perl 5, which permits
 *
 * 1. an unescaped, unbalanced right bracket ]
 * 2. an unescaped, unbalanced left brace    {
 * 3. an unescaped, unbalanced right brace   }
 *
 * If any of these should occur, Perl treats each as a literal
 * character.  Therefore we permit all three of these cases, even
 * though not ECMA-compliant.  Note Perl errors on an unescaped,
 * unbalanced left bracket; so will we.
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=223273
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 223273;
var summary = 'Unescaped, unbalanced parens in regexp should be a SyntaxError';
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
 * All the following contain unescaped, unbalanced parens and
 * should generate SyntaxErrors. That's what we're testing for.
 *
 * To allow the test to compile and run, we have to hide the errors
 * inside eval strings, and check they are caught at run-time.
 *
 * Inside such strings, remember to escape any escape character!
 */
status = inSection(1);
testThis(' /(/ ');

status = inSection(2);
testThis(' /)/ ');

status = inSection(3);
testThis(' /(abc\\)def(g/ ');

status = inSection(4);
testThis(' /\\(abc)def)g/ ');


/*
 * These regexp patterns are correct and should not generate
 * any errors. Note we use checkThis() instead of testThis().
 */
status = inSection(5);
checkThis(' /\\(/ ');

status = inSection(6);
checkThis(' /\\)/ ');

status = inSection(7);
checkThis(' /(abc)def\\(g/ ');

status = inSection(8);
checkThis(' /(abc\\)def)g/ ');

status = inSection(9);
checkThis(' /(abc(\\))def)g/ ');

status = inSection(10);
checkThis(' /(abc([x\\)yz]+)def)g/ ');



/*
 * Unescaped, unbalanced left brackets should be a SyntaxError
 */
status = inSection(11);
testThis(' /[/ ');

status = inSection(12);
testThis(' /[abc\\]def[g/ ');


/*
 * We permit unescaped, unbalanced right brackets, as does Perl.
 * No error should result, even though this is not ECMA-compliant.
 * Note we use checkThis() instead of testThis().
 */
status = inSection(13);
checkThis(' /]/ ');

status = inSection(14);
checkThis(' /\\[abc]def]g/ ');


/*
 * These regexp patterns are correct and should not generate
 * any errors. Note we use checkThis() instead of testThis().
 */
status = inSection(15);
checkThis(' /\\[/ ');

status = inSection(16);
checkThis(' /\\]/ ');

status = inSection(17);
checkThis(' /[abc]def\\[g/ ');

status = inSection(18);
checkThis(' /[abc\\]def]g/ ');

status = inSection(19);
checkThis(' /(abc[\\]]def)g/ ');

status = inSection(20);
checkThis(' /[abc(x\\]yz+)def]g/ ');



/*
 * Run some tests for unbalanced braces. We again follow Perl, and
 * thus permit unescaped unbalanced braces - both left and right,
 * even though this is not ECMA-compliant.
 *
 * Note we use checkThis() instead of testThis().
 */
status = inSection(21);
checkThis(' /abc{def/ ');

status = inSection(22);
checkThis(' /abc}def/ ');

status = inSection(23);
checkThis(' /a{2}bc{def/ ');

status = inSection(24);
checkThis(' /a}b{3}c}def/ ');


/*
 * These regexp patterns are correct and should not generate
 * any errors. Note we use checkThis() instead of testThis().
 */
status = inSection(25);
checkThis(' /abc\\{def/ ');

status = inSection(26);
checkThis(' /abc\\}def/ ');

status = inSection(27);
checkThis(' /a{2}bc\\{def/ ');

status = inSection(28);
checkThis(' /a\\}b{3}c\\}def/ ');




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
 * Valid syntax shouldn't generate any errors
 */
function checkThis(sValidSyntax)
{
  expect = CHECK_PASSED;
  actual = CHECK_PASSED;

  try
  {
    eval(sValidSyntax);
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
