/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    05 June 2003
 * SUMMARY: Testing |with (f)| inside the definition of |function f()|
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=208496
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 208496;
var summary = 'Testing |with (f)| inside the definition of |function f()|';
var status = '';
var statusitems = [];
var actual = '(TEST FAILURE)';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


/*
 * GLOBAL SCOPE
 */
function f(par)
{
  var a = par;

  with(f)
  {
    var b = par;
    actual = b;
  }
}

status = inSection(1);
f('abc'); // this sets |actual|
expect = 'abc';
addThis();

status = inSection(2);
f(111 + 222); // sets |actual|
expect = 333;
addThis();


/*
 * EVAL SCOPE
 */
var s = '';
s += 'function F(par)';
s += '{';
s += '  var a = par;';

s += '  with(F)';
s += '  {';
s += '    var b = par;';
s += '    actual = b;';
s += '  }';
s += '}';

s += 'status = inSection(3);';
s += 'F("abc");'; // sets |actual|
s += 'expect = "abc";';
s += 'addThis();';

s += 'status = inSection(4);';
s += 'F(111 + 222);'; // sets |actual|
s += 'expect = 333;';
s += 'addThis();';
eval(s);


/*
 * FUNCTION SCOPE
 */
function g(par)
{
  // Add outer variables to complicate the scope chain -
  var a = '(TEST FAILURE)';
  var b = '(TEST FAILURE)';
  h(par);

  function h(par)
  {
    var a = par;

    with(h)
    {
      var b = par;
      actual = b;
    }
  }
}

status = inSection(5);
g('abc'); // sets |actual|
expect = 'abc';
addThis();

status = inSection(6);
g(111 + 222); // sets |actual|
expect = 333;
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
