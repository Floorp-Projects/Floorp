/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    25 Nov 2002
 * SUMMARY: Calling a user-defined superconstructor
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=181914, esp. Comment 10.
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = '181914';
var summary = 'Calling a user-defined superconstructor';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];
var EMPTY_STRING = '';
var EXPECTED_FORMAT = 0;


// make a user-defined version of the Error constructor
function _Error(msg)
{
  this.message = msg;
}
_Error.prototype = new Error();
_Error.prototype.name = '_Error';


// derive MyApplyError from _Error
function MyApplyError(msg)
{
  if(this instanceof MyApplyError)
    _Error.apply(this, arguments);
  else
    return new MyApplyError(msg);
}
MyApplyError.prototype = new _Error();
MyApplyError.prototype.name = "MyApplyError";


// derive MyCallError from _Error
function MyCallError(msg)
{
  if(this instanceof MyCallError)
    _Error.call(this, msg);
  else
    return new MyCallError(msg);
}
MyCallError.prototype = new _Error();
MyCallError.prototype.name = "MyCallError";


function otherScope(msg)
{
  return MyApplyError(msg);
}


status = inSection(1);
var err1 = new MyApplyError('msg1');
actual = examineThis(err1, 'msg1');
expect = EXPECTED_FORMAT;
addThis();

status = inSection(2);
var err2 = new MyCallError('msg2');
actual = examineThis(err2, 'msg2');
expect = EXPECTED_FORMAT;
addThis();

status = inSection(3);
var err3 = MyApplyError('msg3');
actual = examineThis(err3, 'msg3');
expect = EXPECTED_FORMAT;
addThis();

status = inSection(4);
var err4 = MyCallError('msg4');
actual = examineThis(err4, 'msg4');
expect = EXPECTED_FORMAT;
addThis();

status = inSection(5);
var err5 = otherScope('msg5');
actual = examineThis(err5, 'msg5');
expect = EXPECTED_FORMAT;
addThis();

status = inSection(6);
var err6 = otherScope();
actual = examineThis(err6, EMPTY_STRING);
expect = EXPECTED_FORMAT;
addThis();

status = inSection(7);
var err7 = eval("MyApplyError('msg7')");
actual = examineThis(err7, 'msg7');
expect = EXPECTED_FORMAT;
addThis();

status = inSection(8);
var err8;
try
{
  throw MyApplyError('msg8');
}
catch(e)
{
  if(e instanceof Error)
    err8 = e;
}
actual = examineThis(err8, 'msg8');
expect = EXPECTED_FORMAT;
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



// Searches |err.toString()| for |err.name + ':' + err.message|
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
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (var i = 0; i < UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
