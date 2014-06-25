/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    01 Feb 2002
 * SUMMARY: Testing Error.length
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=123002
 *
 * NOTE: Error.length should equal the length of FormalParameterList of the
 * Error constructor. This is currently 1 in Rhino, 3 in SpiderMonkey.
 *
 * The difference is due to http://bugzilla.mozilla.org/show_bug.cgi?id=50447.
 * As a result of that bug, SpiderMonkey has extended ECMA to allow two new
 * parameters to Error constructors:
 *
 * Rhino:        new Error (message)
 * SpiderMonkey: new Error (message, fileName, lineNumber)
 *
 * NOTE: since we have hard-coded the length expectations, this testcase will
 * have to be changed if the Error FormalParameterList is ever changed again.
 *
 * To do this, just change the two LENGTH constants below -
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 123002;
var summary = 'Testing Error.length';
var QUOTE = '"';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


var LENGTH_EXPECTED = 1;

/*
 * The various NativeError objects; see ECMA-262 Edition 3, Section 15.11.6
 */
var errObjects = [new Error(), new EvalError(), new RangeError(),
		  new ReferenceError(), new SyntaxError(), new TypeError(), new URIError()];


for (var i in errObjects)
{
  var err = errObjects[i];
  status = inSection(quoteThis(err.name));
  actual = Error.length;
  expect = LENGTH_EXPECTED;
  addThis();
}



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


function quoteThis(text)
{
  return QUOTE + text + QUOTE;
}
