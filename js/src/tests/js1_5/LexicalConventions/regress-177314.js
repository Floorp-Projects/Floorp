/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    30 Oct 2002
 * SUMMARY: '\400' should lex as a 2-digit octal escape + '0'
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=177314
 *
 * Bug was that Rhino interpreted '\400' as a 3-digit octal escape. As such
 * it is invalid, since octal escapes may only run from '\0' to '\377'. But
 * the lexer should interpret this as '\40' + '0' instead, and throw no error.
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 177314;
var summary = "'\\" + "400' should lex as a 2-digit octal escape + '0'";
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


// the last valid octal escape is '\377', which should equal hex escape '\xFF'
status = inSection(1);
actual = '\377';
expect = '\xFF';
addThis();

// now exercise the lexer by going one higher in the last digit
status = inSection(2);
actual = '\378';
expect = '\37' + '8';
addThis();

// trickier: 400 is a valid octal number, but '\400' isn't a valid octal escape
status = inSection(3);
actual = '\400';
expect = '\40' + '0';
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
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
