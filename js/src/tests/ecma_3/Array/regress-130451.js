/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    25 Mar 2002
 * SUMMARY: Array.prototype.sort() should not (re-)define .length
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=130451
 *
 * From the ECMA-262 Edition 3 Final spec:
 *
 * NOTE: The sort function is intentionally generic; it does not require that
 * its |this| value be an Array object. Therefore, it can be transferred to
 * other kinds of objects for use as a method. Whether the sort function can
 * be applied successfully to a host object is implementation-dependent.
 *
 * The interesting parts of this testcase are the contrasting expectations for
 * Brendan's test below, when applied to Array objects vs. non-Array objects.
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 130451;
var summary = 'Array.prototype.sort() should not (re-)define .length';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];
var arr = [];
var cmp = new Function();


/*
 * First: test Array.prototype.sort() on Array objects
 */
status = inSection(1);
arr = [0,1,2,3];
cmp = function(x,y) {return x-y;};
actual = arr.sort(cmp).length;
expect = 4;
addThis();

status = inSection(2);
arr = [0,1,2,3];
cmp = function(x,y) {return y-x;};
actual = arr.sort(cmp).length;
expect = 4;
addThis();

status = inSection(3);
arr = [0,1,2,3];
cmp = function(x,y) {return x-y;};
arr.length = 1;
actual = arr.sort(cmp).length;
expect = 1;
addThis();

/*
 * This test is by Brendan. Setting arr.length to
 * 2 and then 4 should cause elements to be deleted.
 */
arr = [0,1,2,3];
cmp = function(x,y) {return x-y;};
arr.sort(cmp);

status = inSection(4);
actual = arr.join();
expect = '0,1,2,3';
addThis();

status = inSection(5);
actual = arr.length;
expect = 4;
addThis();

status = inSection(6);
arr.length = 2;
actual = arr.join();
expect = '0,1';
addThis();

status = inSection(7);
arr.length = 4;
actual = arr.join();
expect = '0,1,,';  //<---- see how 2,3 have been lost
addThis();



/*
 * Now test Array.prototype.sort() on non-Array objects
 */
status = inSection(8);
var obj = new Object();
obj.sort = Array.prototype.sort;
obj.length = 4;
obj[0] = 0;
obj[1] = 1;
obj[2] = 2;
obj[3] = 3;
cmp = function(x,y) {return x-y;};
actual = obj.sort(cmp).length;
expect = 4;
addThis();


/*
 * Here again is Brendan's test. Unlike the array case
 * above, the setting of obj.length to 2 and then 4
 * should NOT cause elements to be deleted
 */
obj = new Object();
obj.sort = Array.prototype.sort;
obj.length = 4;
obj[0] = 3;
obj[1] = 2;
obj[2] = 1;
obj[3] = 0;
cmp = function(x,y) {return x-y;};
obj.sort(cmp);  //<---- this is what triggered the buggy behavior below
obj.join = Array.prototype.join;

status = inSection(9);
actual = obj.join();
expect = '0,1,2,3';
addThis();

status = inSection(10);
actual = obj.length;
expect = 4;
addThis();

status = inSection(11);
obj.length = 2;
actual = obj.join();
expect = '0,1';
addThis();

/*
 * Before this bug was fixed, |actual| held the value '0,1,,'
 * as in the Array-object case at top. This bug only occurred
 * if Array.prototype.sort() had been applied to |obj|,
 * as we have done higher up.
 */
status = inSection(12);
obj.length = 4;
actual = obj.join();
expect = '0,1,2,3';
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
