/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    23 Nov 2002
 * SUMMARY: Calling toString for an object derived from the Error class
 *		   results in an TypeError (Rhino only)
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=181654
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = '181654';
var summary = 'Calling toString for an object derived from the Error class should be possible.';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];
var EMPTY_STRING = '';
var EXPECTED_FORMAT = 0;


// derive MyError from Error
function MyError( msg )
{
  this.message = msg;
}
MyError.prototype = new Error();
MyError.prototype.name = "MyError";


status = inSection(1);
var err1 = new MyError('msg1');
actual = examineThis(err1, 'msg1');
expect = EXPECTED_FORMAT;
addThis();

status = inSection(2);
var err2 = new MyError(String(err1));
actual = examineThis(err2, err1);
expect = EXPECTED_FORMAT;
addThis();

status = inSection(3);
var err3 = new MyError();
actual = examineThis(err3, EMPTY_STRING);
expect = EXPECTED_FORMAT;
addThis();

status = inSection(4);
var err4 = new MyError(EMPTY_STRING);
actual = examineThis(err4, EMPTY_STRING);
expect = EXPECTED_FORMAT;
addThis();

// now generate an error -
status = inSection(5);
try
{
  throw new MyError("thrown");
}
catch(err5)
{
  actual = examineThis(err5, "thrown");
}
expect = EXPECTED_FORMAT;
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



/*
 * Searches err.toString() for err.name + ':' + err.message,
 * with possible whitespace on each side of the colon sign.
 *
 * We allow for no colon in case err.message was not provided by the user.
 * In such a case, SpiderMonkey and Rhino currently set err.message = '',
 * as allowed for by ECMA 15.11.4.3. This makes |pattern| work in this case.
 *
 * If this is ever changed to a non-empty string, e.g. 'undefined',
 * you may have to modify |pattern| to take that into account -
 *
 */
function examineThis(err, msg)
{
  var pattern = err.name + '\\s*:?\\s*' + msg;
  return err.toString().search(RegExp(pattern));
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
