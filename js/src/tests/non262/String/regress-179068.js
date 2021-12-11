/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    09 November 2002
 * SUMMARY: Test that interpreter can handle string literals exceeding 64K
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=179068
 *
 * Test that the interpreter can handle string literals exceeding 64K limit.
 * For that the script passes to eval() "str ='LONG_STRING_LITERAL';" where
 * LONG_STRING_LITERAL is a string with 200K chars.
 *
 *   Igor Bukanov explains the technique used below:
 *
 * > Philip Schwartau wrote:
 * >...
 * > Here is the heart of the testcase:
 * >
 * >   // Generate 200K long string
 * >   var long_str = duplicate(LONG_STR_SEED, N);
 * >   var str = "";
 * >   eval("str='".concat(long_str, "';"));
 * >   var test_is_ok = (str.length == LONG_STR_SEED.length * N);
 * >
 * >
 * > The testcase creates two identical strings, |long_str| and |str|. It
 * > uses eval() simply to assign the value of |long_str| to |str|. Why is
 * > it necessary to have the variable |str|, then? Why not just create
 * > |long_str| and test it? Wouldn't this be enough:
 * >
 * >   // Generate 200K long string
 * >   var long_str = duplicate(LONG_STR_SEED, N);
 * >   var test_is_ok = (long_str.length == LONG_STR_SEED.length * N);
 * >
 * > Or do we specifically need to test eval() to exercise the interpreter?
 *
 * The reason for eval is to test string literals like in 'a string literal
 * with 100 000 characters...', Rhino deals fine with strings generated at
 * run time where lengths > 64K. Without eval it would be necessary to have
 * a test file excedding 64K which is not that polite for CVS and then a
 * special treatment for the compiled mode in Rhino should be added.
 *
 *
 * >
 * > If so, is it important to use the concat() method in the assignment, as
 * > you have done: |eval("str='".concat(long_str, "';"))|, or can we simply
 * > do |eval("str = long_str;")| ?
 *
 * The concat is a replacement for eval("str='"+long_str+"';"), but as
 * long_str is huge, this leads to constructing first a new string via
 * "str='"+long_str and then another one via ("str='"+long_str) + "';"
 * which takes time under JDK 1.1 on a something like StrongArm 200MHz.
 * Calling concat makes less copies, that is why it is used in the
 * duplicate function and this is faster then doing recursion like in the
 * test case to test that 64K different string literals can be handled.
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 179068;
var summary = 'Test that interpreter can handle string literals exceeding 64K';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];
var LONG_STR_SEED = "0123456789";
var N = 20 * 1024;
var str = "";


// Generate 200K long string and assign it to |str| via eval()
var long_str = duplicate(LONG_STR_SEED, N);
eval("str='".concat(long_str, "';"));

status = inSection(1);
actual = str.length == LONG_STR_SEED.length * N
  expect = true;
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function duplicate(str, count)
{
  var tmp = new Array(count);

  while (count != 0)
    tmp[--count] = str;

  return String.prototype.concat.apply("", tmp);
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
