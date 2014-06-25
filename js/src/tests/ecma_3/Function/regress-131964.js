/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    19 Mar 2002
 * SUMMARY: Function declarations in global or function scope are {DontDelete}.
 *          Function declarations in eval scope are not {DontDelete}.
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=131964
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER =   131964;
var summary = 'Functions defined in global or function scope are {DontDelete}';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


status = inSection(1);
function f()
{
  return 'f lives!';
}
delete f;

try
{
  actual = f();
}
catch(e)
{
  actual = 'f was deleted';
}

expect = 'f lives!';
addThis();



/*
 * Try the same test in function scope -
 */
status = inSection(2);
function g()
{
  function f()
  {
    return 'f lives!';
  }
  delete f;

  try
  {
    actual = f();
  }
  catch(e)
  {
    actual = 'f was deleted';
  }

  expect = 'f lives!';
  addThis();
}
g();



/*
 * Try the same test in eval scope - here we EXPECT the function to be deleted (?)
 */
status = inSection(3);
var s = '';
s += 'function h()';
s += '{ ';
s += '  return "h lives!";';
s += '}';
s += 'delete h;';

s += 'try';
s += '{';
s += '  actual = h();';
s += '}';
s += 'catch(e)';
s += '{';
s += '  actual = "h was deleted";';
s += '}';

s += 'expect = "h was deleted";';
s += 'addThis();';
eval(s);


/*
 * Define the function in eval scope, but delete it in global scope -
 */
status = inSection(4);
s = '';
s += 'function k()';
s += '{ ';
s += '  return "k lives!";';
s += '}';
eval(s);

delete k;

try
{
  actual = k();
}
catch(e)
{
  actual = 'k was deleted';
}

expect = 'k was deleted';
addThis();




//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function wasDeleted(functionName)
{
  return functionName + ' was deleted...';
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
  enterFunc('test');
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
