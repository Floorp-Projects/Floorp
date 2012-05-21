/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    06 February 2003
 * SUMMARY: Using |instanceof| to check if function is called as a constructor
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=192105
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 192105;
var summary = 'Using |instanceof| to check if f() is called as constructor';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


/*
 * This function is the heart of the test. It sets the result
 * variable |actual|, which we will compare against |expect|.
 *
 * Note |actual| will be set to |true| or |false| according
 * to whether or not this function is called as a constructor;
 * i.e. whether it is called via the |new| keyword or not -
 */
function f()
{
  actual = (this instanceof f);
}


/*
 * Call f as a constructor from global scope
 */
status = inSection(1);
new f(); // sets |actual|
expect = true;
addThis();

/*
 * Now, not as a constructor
 */
status = inSection(2);
f(); // sets |actual|
expect = false;
addThis();


/*
 * Call f as a constructor from function scope
 */
function F()
{
  new f();
}
status = inSection(3);
F(); // sets |actual|
expect = true;
addThis();

/*
 * Now, not as a constructor
 */
function G()
{
  f();
}
status = inSection(4);
G(); // sets |actual|
expect = false;
addThis();


/*
 * Now make F() and G() methods of an object
 */
var obj = {F:F, G:G};
status = inSection(5);
obj.F(); // sets |actual|
expect = true;
addThis();

status = inSection(6);
obj.G(); // sets |actual|
expect = false;
addThis();


/*
 * Now call F() and G() from yet other functions, and use eval()
 */
function A()
{
  eval('F();');
}
status = inSection(7);
A(); // sets |actual|
expect = true;
addThis();


function B()
{
  eval('G();');
}
status = inSection(8);
B(); // sets |actual|
expect = false;
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
