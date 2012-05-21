/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    22 Aug 2009
 * SUMMARY: Utf8ToOneUcs4Char in jsstr.cpp ,overlong UTF-8 seqence detection problem
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=511859
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 511859;
var summary = 'Utf8ToOneUcs4Char in jsstr.cpp ,overlong UTF-8 seqence detection problem';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect =  '';
var expectedvalues = [];
var arg;
/*
 * The patch for http://bugzilla.mozilla.org/show_bug.cgi?id=511859
 * defined this value to be the result of an overlong UTF-8 sequence -
 */

var EX="(Exception thrown)";

function getActual(s)
{
  try {
    return decodeURI(s);
  } catch (e) {
    return EX;
  }
}

//Phase1: overlong sequences
expect = EX;
arg = "%C1%BF";
status = "Overlong 2byte U+7f :" + arg;
actual = getActual(arg);
addThis();

arg = "%E0%9F%BF";
status = "Overlong 3byte U+7ff :" + arg;
actual = getActual(arg);
addThis();

arg = "%F0%88%80%80";
status = "Overlong 4byte U+8000 :" + arg;
actual = getActual(arg);
addThis();

arg = "%F0%8F%BF%BF";
status = "Overlong 4byte U+ffff :" + arg;
actual = getActual(arg);
addThis();

arg = "%F0%80%C0%80%80";
status = "Overlong 5byte U+20000 :" + arg;
actual = getActual(arg);
addThis();

arg = "%F8%84%8F%BF%BF";
status = "Overlong 5byte U+10FFFF :" + arg;
actual = getActual(arg);
addThis();

arg = "%FC%80%84%8F%BF%BF";
status = "Overlong 6byte U+10FFFF :" + arg;
actual = getActual(arg);
addThis();

//Phase 2:Out of Unicode range
arg = "%F4%90%80%80%80";
status = "4byte 0x110000 :" + arg;
actual = getActual(arg);
addThis();
arg = "%F8%84%90%80%80";
status = "5byte 0x110000 :" + arg;
actual = getActual(arg);
addThis();

arg = "%FC%80%84%90%80%80";
status = "6byte 0x110000 :" + arg;
actual = getActual(arg);
addThis();

//Phase 3:Valid sequences must be decoded correctly
arg = "%7F";
status = "valid sequence U+7F :" + arg;
actual = getActual("%7F");
expect = "\x7f";
addThis();

arg = "%C2%80";
status = "valid sequence U+80 :" + arg;
actual = getActual(arg);
expect = "\x80";
addThis();

arg = "%E0%A0%80";
status = "valid sequence U+800 :" + arg;
actual = getActual("%E0%A0%80");
expect = "\u0800";
addThis();

arg = "%F0%90%80%80"
status = "valid sequence U+10000 :" + arg;
actual = getActual(arg);
expect = "\uD800\uDC00";
addThis();

arg = "%F4%8F%BF%BF";
status = "valid sequence U+10FFFF :" + arg;
actual = getActual(arg);
expect = "\uDBFF\uDFFF";
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

  for (var i=5; i<=9; i++)
    status = summary + ': UTF-8 test: bad UTF-8 sequence ' + i;
    expect = 'Error';
    actual = 'No error!';
    try
    {
      testUTF8(i);
    }
    catch (e)
    {
      actual = 'Error';
    }
    reportCompare(expect, actual, status);

  exitFunc('test');
}
