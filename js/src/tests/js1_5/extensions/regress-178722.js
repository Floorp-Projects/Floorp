/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    06 November 2002
 * SUMMARY: arr.sort() should not output |undefined| when |arr| is empty
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=178722
 *
 * ECMA-262 Ed.3: 15.4.4.11 Array.prototype.sort (comparefn)
 *
 * 1. Call the [[Get]] method of this object with argument "length".
 * 2. Call ToUint32(Result(1)).
 * 3. Perform an implementation-dependent sequence of calls to the [[Get]],
 *    [[Put]], and [[Delete]] methods of this object, etc. etc.
 * 4. Return this object.
 *
 *
 * Note that sort() is done in-place on |arr|. In other words, sort() is a
 * "destructive" method rather than a "functional" method. The return value
 * of |arr.sort()| and |arr| are the same object.
 *
 * If |arr| is an empty array, the return value of |arr.sort()| should be
 * an empty array, not the value |undefined| as was occurring in bug 178722.
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 178722;
var summary = 'arr.sort() should not output |undefined| when |arr| is empty';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];
var arr;


// create empty array or pseudo-array objects in various ways
function f () {return arguments};
var arr5 = f();
arr5.__proto__ = Array.prototype;


status = inSection(5);
arr = arr5.sort();
actual = arr instanceof Array && arr.length === 0 && arr === arr5;
expect = true;
addThis();


// now do the same thing, with non-default sorting:
function g() {return 1;}

status = inSection('5a');
arr = arr5.sort(g);
actual = arr instanceof Array && arr.length === 0 && arr === arr5;
expect = true;
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
