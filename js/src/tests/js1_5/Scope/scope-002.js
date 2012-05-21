/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 2001-07-02
 *
 * SUMMARY:  Testing visibility of outer function from inner function.
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = '(none)';
var summary = 'Testing visibility of outer function from inner function';
var cnCousin = 'Fred';
var cnColor = 'red';
var cnMake = 'Toyota';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


// TEST 1
function Outer()
{

  function inner()
  {
    Outer.cousin = cnCousin;
    return Outer.cousin;
  }

  status = 'Section 1 of test';
  actual = inner();
  expect = cnCousin;
  addThis();
}


Outer();
status = 'Section 2 of test';
actual = Outer.cousin;
expect = cnCousin;
addThis();



// TEST 2
function Car(make)
{
  this.make = make;
  Car.prototype.paint = paint;

  function paint()
  {
    Car.color = cnColor;
    Car.prototype.color = Car.color;
  }
}


var myCar = new Car(cnMake);
status = 'Section 3 of test';
actual = myCar.make;
expect = cnMake;
addThis();


myCar.paint();
status = 'Section 4 of test';
actual = myCar.color;
expect = cnColor;
addThis();



//--------------------------------------------------
test();
//--------------------------------------------------



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

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
