/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 351463;
var summary = 'Treat hyphens as not special adjacent to CharacterClassEscapes in character classes';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  var r;
  var s = 'a0- z';

  r = '([\\d-\\s]+)';
  expect = ['0- ', '0- '] + '';
  actual = null;

  try
  {
    actual = new RegExp(r).exec(s) + '';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': /' + r + '/.exec("' + s + '")');

  r = '([\\s-\\d]+)';
  expect = ['0- ', '0- '] + '';
  actual = null;

  try
  {
    actual = new RegExp(r).exec(s) + '';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': /' + r + '/.exec("' + s + '")');

  r = '([\\D-\\s]+)';
  expect = ['a', 'a'] + '';
  actual = null;

  try
  {
    actual = new RegExp(r).exec(s) + '';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': /' + r + '/.exec("' + s + '")');

  r = '([\\s-\\D]+)';
  expect = ['a', 'a'] + '';
  actual = null;

  try
  {
    actual = new RegExp(r).exec(s) + '';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': /' + r + '/.exec("' + s + '")');

  r = '([\\d-\\S]+)';
  expect = ['a0-', 'a0-'] + '';
  actual = null;

  try
  {
    actual = new RegExp(r).exec(s) + '';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': /' + r + '/.exec("' + s + '")');

  r = '([\\S-\\d]+)';
  expect = ['a0-', 'a0-'] + '';
  actual = null;

  try
  {
    actual = new RegExp(r).exec(s) + '';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': /' + r + '/.exec("' + s + '")');

  r = '([\\D-\\S]+)';
  expect = ['a0- z', 'a0- z'] + '';
  actual = null;

  try
  {
    actual = new RegExp(r).exec(s) + '';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': /' + r + '/.exec("' + s + '")');

  r = '([\\S-\\D]+)';
  expect = ['a0- z', 'a0- z'] + '';
  actual = null;

  try
  {
    actual = new RegExp(r).exec(s) + '';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': /' + r + '/.exec("' + s + '")');

// --

  r = '([\\w-\\s]+)';
  expect = ['a0- z', 'a0- z'] + '';
  actual = null;

  try
  {
    actual = new RegExp(r).exec(s) + '';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': /' + r + '/.exec("' + s + '")');

  r = '([\\s-\\w]+)';
  expect = ['a0- z', 'a0- z'] + '';
  actual = null;

  try
  {
    actual = new RegExp(r).exec(s) + '';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': /' + r + '/.exec("' + s + '")');

  r = '([\\W-\\s]+)';
  expect = ['- ', '- '] + '';
  actual = null;

  try
  {
    actual = new RegExp(r).exec(s) + '';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': /' + r + '/.exec("' + s + '")');

  r = '([\\s-\\W]+)';
  expect = ['- ', '- '] + '';
  actual = null;

  try
  {
    actual = new RegExp(r).exec(s) + '';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': /' + r + '/.exec("' + s + '")');

  r = '([\\w-\\S]+)';
  expect = ['a0-', 'a0-'] + '';
  actual = null;

  try
  {
    actual = new RegExp(r).exec(s) + '';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': /' + r + '/.exec("' + s + '")');

  r = '([\\S-\\w]+)';
  expect = ['a0-', 'a0-'] + '';
  actual = null;

  try
  {
    actual = new RegExp(r).exec(s) + '';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': /' + r + '/.exec("' + s + '")');

  r = '([\\W-\\S]+)';
  expect = ['a0- z', 'a0- z'] + '';
  actual = null;

  try
  {
    actual = new RegExp(r).exec(s) + '';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': /' + r + '/.exec("' + s + '")');

  r = '([\\S-\\W]+)';
  expect = ['a0- z', 'a0- z'] + '';
  actual = null;

  try
  {
    actual = new RegExp(r).exec(s) + '';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': /' + r + '/.exec("' + s + '")');
}
