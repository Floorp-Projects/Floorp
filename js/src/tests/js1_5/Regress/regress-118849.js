/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    08 Jan 2002
 * SUMMARY: Just testing that we don't crash on this code
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=118849
 *
 * http://developer.netscape.com:80/docs/manuals/js/core/jsref/function.htm
 * The Function constructor:
 * Function ([arg1[, arg2[, ... argN]],] functionBody)
 *
 * Parameters
 * arg1, arg2, ... argN
 *   (Optional) Names to be used by the function as formal argument names.
 *   Each must be a string that corresponds to a valid JavaScript identifier.
 *
 * functionBody
 *   A string containing JS statements comprising the function definition.
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 118849;
var summary = 'Should not crash if we provide Function() with bad arguments'
  var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];
var cnFAIL_1 = 'LEGAL call to Function() caused an ERROR!!!';
var cnFAIL_2 = 'ILLEGAL call to Function() FAILED to cause an error';
var cnSTRING = 'ASDF';
var cnNUMBER = 123;


/***********************************************************/
/****  THESE ARE LEGITMATE CALLS AND SHOULD ALL SUCCEED  ***/
/***********************************************************/
status = inSection(1);
actual = cnFAIL_1; // initialize to failure
try
{
  Function(cnSTRING);
  Function(cnNUMBER);  // cnNUMBER is a valid functionBody       
  Function(cnSTRING,cnSTRING);
  Function(cnSTRING,cnNUMBER);
  Function(cnSTRING,cnSTRING,cnNUMBER);

  new Function(cnSTRING);
  new Function(cnNUMBER);
  new Function(cnSTRING,cnSTRING);
  new Function(cnSTRING,cnNUMBER);
  new Function(cnSTRING,cnSTRING,cnNUMBER);

  actual = expect;
}
catch(e)
{
}
addThis();



/**********************************************************/
/***  EACH CASE THAT FOLLOWS SHOULD TRIGGER AN ERROR    ***/
/***               (BUT NOT A CRASH)                    ***/
/***  NOTE WE NOW USE cnFAIL_2 INSTEAD OF cnFAIL_1      ***/
/**********************************************************/
status = inSection(2);
actual = cnFAIL_2;
try
{
  Function(cnNUMBER,cnNUMBER); // cnNUMBER is an invalid JS identifier name
}
catch(e)
{
  actual = expect;
}
addThis();


status = inSection(3);
actual = cnFAIL_2;
try
{
  Function(cnNUMBER,cnSTRING,cnSTRING);
}
catch(e)
{
  actual = expect;
}
addThis();


status = inSection(4);
actual = cnFAIL_2;
try
{
  new Function(cnNUMBER,cnNUMBER);
}
catch(e)
{
  actual = expect;
}
addThis();


status = inSection(5);
actual = cnFAIL_2;
try
{
  new Function(cnNUMBER,cnSTRING,cnSTRING);
}
catch(e)
{
  actual = expect;
}
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
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (var i = 0; i < UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
