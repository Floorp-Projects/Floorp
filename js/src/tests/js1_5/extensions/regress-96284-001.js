/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 03 September 2001
 *
 * SUMMARY: Double quotes should be escaped in Error.prototype.toSource()
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=96284
 *
 * The real point here is this: we should be able to reconstruct an object
 * from its toSource() property. We'll test this on various types of objects.
 *
 * Method: define obj2 = eval(obj1.toSource()) and verify that
 * obj2.toSource() == obj1.toSource().
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 96284;
var summary = 'Double quotes should be escaped in Error.prototype.toSource()';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];
var obj1 = {};
var obj2 = {};
var cnTestString = '"This is a \" STUPID \" test string!!!"\\';


// various NativeError objects -
status = inSection(1);
obj1 = Error(cnTestString);
obj2 = eval(obj1.toSource());
actual = obj2.toSource();
expect = obj1.toSource();
addThis();

status = inSection(2);
obj1 = EvalError(cnTestString);
obj2 = eval(obj1.toSource());
actual = obj2.toSource();
expect = obj1.toSource();
addThis();

status = inSection(3);
obj1 = RangeError(cnTestString);
obj2 = eval(obj1.toSource());
actual = obj2.toSource();
expect = obj1.toSource();
addThis();

status = inSection(4);
obj1 = ReferenceError(cnTestString);
obj2 = eval(obj1.toSource());
actual = obj2.toSource();
expect = obj1.toSource();
addThis();

status = inSection(5);
obj1 = SyntaxError(cnTestString);
obj2 = eval(obj1.toSource());
actual = obj2.toSource();
expect = obj1.toSource();
addThis();

status = inSection(6);
obj1 = TypeError(cnTestString);
obj2 = eval(obj1.toSource());
actual = obj2.toSource();
expect = obj1.toSource();
addThis();

status = inSection(7);
obj1 = URIError(cnTestString);
obj2 = eval(obj1.toSource());
actual = obj2.toSource();
expect = obj1.toSource();
addThis();


// other types of objects -
status = inSection(8);
obj1 = new String(cnTestString);
obj2 = eval(obj1.toSource());
actual = obj2.toSource();
expect = obj1.toSource();
addThis();

status = inSection(9);
obj1 = {color:'red', texture:cnTestString, hasOwnProperty:42};
obj2 = eval(obj1.toSource());
actual = obj2.toSource();
expect = obj1.toSource();
addThis();

status = inSection(10);
obj1 = function(x) {function g(y){return y+1;} return g(x);};
obj2 = eval(obj1.toSource());
actual = obj2.toSource();
expect = obj1.toSource();
addThis();

status = inSection(11);
obj1 = new Number(eval('6'));
obj2 = eval(obj1.toSource());
actual = obj2.toSource();
expect = obj1.toSource();
addThis();

status = inSection(12);
obj1 = /ad;(lf)kj(2309\/\/)\/\//;
obj2 = eval(obj1.toSource());
actual = obj2.toSource();
expect = obj1.toSource();
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
