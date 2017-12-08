/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 231518;
var summary = 'decompiler must quote keywords and non-identifier property names';
var actual = '';
var expect = 'no error';
var status;
var object;
var result;

printBugNumber(BUGNUMBER);
printStatus (summary);


if (typeof uneval != 'undefined')
{
  status = inSection(1) + ' eval(uneval({"if": false}))';

  try
  {
    object = {'if': false };
    result = uneval(object);
    printStatus('uneval returns ' + result);
    eval(result);
    actual = 'no error';
  }
  catch(e)
  {
    actual = 'error';
  }
 
  reportCompare(expect, actual, status);

  status = inSection(2) + ' eval(uneval({"if": "then"}))';

  try
  {
    object = {'if': "then" };
    result = uneval(object);
    printStatus('uneval returns ' + result);
    eval(result);
    actual = 'no error';
  }
  catch(e)
  {
    actual = 'error';
  }
 
  reportCompare(expect, actual, status);

  status = inSection(3) + ' eval(uneval(f))';

  try
  {
    result = uneval(f);
    printStatus('uneval returns ' + result);
    eval(result);
    actual = 'no error';
  }
  catch(e)
  {
    actual = 'error';
  }
 
  reportCompare(expect, actual, status);

  status = inSection(2) + ' eval(uneval(g))';

  try
  {
    result = uneval(g);
    printStatus('uneval returns ' + result);
    eval(result);
    actual = 'no error';
  }
  catch(e)
  {
    actual = 'error';
  }
 
  reportCompare(expect, actual, status);
}

function f()
{
  var obj = new Object();

  obj['name']      = 'Just a name';
  obj['if']        = false;
  obj['some text'] = 'correct';
}

function g()
{
  return {'if': "then"};
}

