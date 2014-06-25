/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
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
  try {
    actual = o.bbb();
  } catch(e) {
    actual = e + '';
  }
  expect = 'TypeError: o.bbb is not a function';
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(3) + ' ';
  try {
    actual = o.ccc();
  } catch(e) {
    actual = e + '';
  }
  expect = 'TypeError: o.ccc is not a function';
  reportCompare(expect, actual, status);

  status = summary + ' ' + inSection(4) + ' ';
  try {
    actual = o.ddd();
  } catch(e) {
    actual = e + '';
  }
  expect = 'TypeError: o.ddd is not a function';
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
