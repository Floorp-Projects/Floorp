/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Matthew Draper <matthew@trebex.net>
 */

var gTestfile = 'regress-564577.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 564577;
var summary = '__noSuchMethod__ when property exists';
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

 
  var o = {
    aaa: undefined,
    bbb: null,
    ccc: 77,
    ddd: 'foo',
    eee: {},
    fff: /./,
    __noSuchMethod__: function (id, args)
    {
      return(id + '('+args.join(',')+') ' + this[id]);
    }
  };

  status = summary + ' ' + inSection(1) + ' ';
  actual = o.aaa();
  expect = 'aaa() undefined';
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(2) + ' ';
  actual = o.bbb();
  expect = 'bbb() null';
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(3) + ' ';
  actual = o.ccc();
  expect = 'ccc() 77';
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(4) + ' ';
  actual = o.ddd();
  expect = 'ddd() foo';
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(5) + ' ';
  try {
    actual = o.eee();
  } catch(e) {
    actual = e + '';
  }
  expect = 'TypeError: o.eee is not a function';
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(6) + ' ';
  try {
    actual = o.fff('xyz') + '';
  } catch(e) {
    actual = e + '';
  }
  expect = 'TypeError: o.fff is not a function';
  reportCompare(expect, actual, status);

  exitFunc('test');
}
