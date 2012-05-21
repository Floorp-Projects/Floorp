/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    14 Jan 2002
 * SUMMARY: Shouldn't crash on regexps with many nested parentheses
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=119909
 *
 */
//-----------------------------------------------------------------------------
var BUGNUMBER = 119909;
var summary = "Shouldn't crash on regexps with many nested parentheses";
var NO_BACKREFS = false;
var DO_BACKREFS = true;


//--------------------------------------------------
test();
//--------------------------------------------------


function test()
{
  enterFunc('test');
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  testThis(500, NO_BACKREFS, 'hello', 'goodbye');
  testThis(500, DO_BACKREFS, 'hello', 'goodbye');

  reportCompare('No Crash', 'No Crash', '');

  exitFunc('test');
}


/*
 * Creates a regexp pattern like (((((((((hello)))))))))
 * and tests str.search(), str.match(), str.replace()
 */
function testThis(numParens, doBackRefs, strOriginal, strReplace)
{
  var openParen = doBackRefs? '(' : '(?:';
  var closeParen = ')';
  var pattern = '';
 
  for (var i=0; i<numParens; i++) {pattern += openParen;}
  pattern += strOriginal;
  for (i=0; i<numParens; i++) {pattern += closeParen;}
  var re = new RegExp(pattern);

  var res = strOriginal.search(re);
  res = strOriginal.match(re);
  res = strOriginal.replace(re, strReplace);
}
