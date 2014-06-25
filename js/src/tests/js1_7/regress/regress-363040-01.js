/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 363040;
var summary = 'Array.prototype.reduce, Array.prototype.reduceRight';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  function f(x,y) { return '(' + x + '+' + y + ')';}

  var testdesc;
  var arr0elms  = [];
  var arr1elms = [1];
  var arr2elms  = [1, 2];

  testdesc = 'Test reduce of empty array without initializer.';
  try
  {
    expect = 'TypeError: reduce of empty array with no initial value';
    arr0elms.reduce(f);
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect + '', actual + '', testdesc + ' : ' + expect);

  testdesc = 'Test reduceRight of empty array without initializer.';
  try
  {
    expect = 'TypeError: reduce of empty array with no initial value';
    arr0elms.reduceRight(f);
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect + '', actual + '', testdesc + ' : ' + expect);

  testdesc = 'Test reduce of empty array with initial value.';
  expect = 'a';
  actual = arr0elms.reduce(f, 'a');
  reportCompare(expect + '', actual + '', testdesc + ' : ' + expect);

  testdesc = 'Test reduceRight of empty array with initial value.';
  expect = 'a';
  actual = arr0elms.reduceRight(f, 'a');
  reportCompare(expect + '', actual + '', testdesc +' : ' + expect);

  testdesc = 'Test reduce of 1 element array with no initializer.';
  expect = '1';
  actual = arr1elms.reduce(f);
  reportCompare(expect + '', actual + '', testdesc + ' : ' + expect);

  testdesc = 'Test reduceRight of 1 element array with no initializer.';
  expect = '1';
  actual = arr1elms.reduceRight(f);
  reportCompare(expect + '', actual + '', testdesc + ' : ' + expect);

  testdesc = 'Test reduce of 2 element array with no initializer.';
  expect = '(1+2)';
  actual = arr2elms.reduce(f);
  reportCompare(expect + '', actual + '', testdesc + ' : ' + expect);

  testdesc = 'Test reduce of 2 element array with initializer.';
  expect = '((a+1)+2)';
  actual = arr2elms.reduce(f,'a');
  reportCompare(expect + '', actual + '', testdesc + ' : ' + expect);

  testdesc = 'Test reduceRight of 2 element array with no initializer.';
  expect = '(2+1)';
  actual = arr2elms.reduceRight(f);
  reportCompare(expect + '', actual + '', testdesc + ' : ' + expect);

  testdesc = 'Test reduceRight of 2 element array with no initializer.';
  expect = '((a+2)+1)';
  actual = arr2elms.reduceRight(f,'a');
  reportCompare(expect + '', actual + '', testdesc + ' : ' + expect);

  exitFunc ('test');
}
