/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 885553;
var summary = 'Array.prototype.find and Array.prototype.findIndex';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function isString(v, index, array)
{
  assertEq(array[index], v);
  return typeof v == 'string';
}

function dumpError(e)
{
  var s = e.name + ': ' + e.message +
    ' File: ' + e.fileName +
    ', Line: ' + e.lineNumber +
    ', Stack: ' + e.stack;
  return s;
}

var expect;
var actual;
var obj;

var strings = ['hello', 'Array', 'WORLD'];
var mixed   = [0, '1', 2];
var sparsestrings = new Array();
sparsestrings[2] = 'sparse';
var arraylike = {0:0, 1:'string', 2:2, length:3};
// array for which JSObject::isIndexed() holds.
var indexedArray = [];
Object.defineProperty(indexedArray, 42, { get: function() { return 42; } });
Object.defineProperty(indexedArray, 142, { get: function() { return 'string'; } });

// find and findIndex have 1 required argument

expect = 1;
actual = Array.prototype.find.length;
reportCompare(expect, actual, 'Array.prototype.find.length == 1');
actual = Array.prototype.findIndex.length;
reportCompare(expect, actual, 'Array.prototype.findIndex.length == 1');

// throw TypeError if no predicate specified
expect = 'TypeError';
try
{
  strings.find();
  actual = 'no error';
}
catch(e)
{
  actual = e.name;
}
reportCompare(expect, actual, 'Array.find(undefined) throws TypeError');
try
{
  strings.findIndex();
  actual = 'no error';
}
catch(e)
{
  actual = e.name;
}
reportCompare(expect, actual, 'Array.findIndex(undefined) throws TypeError');

// Length gets treated as integer, not uint32
obj = { length: -4294967295, 0: 42 };
expected = undefined;
actual = Array.prototype.find.call(obj, () => true);
reportCompare(expected, actual, 'find correctly treats "length" as an integer');
expected = -1
actual = Array.prototype.findIndex.call(obj, () => true);
reportCompare(expected, actual, 'findIndex correctly treats "length" as an integer');

// test find and findIndex results
try
{
  expect = 'hello';
  actual = strings.find(isString);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, 'strings: find finds first string element');

try
{
  expect = 0;
  actual = strings.findIndex(isString);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, 'strings: findIndex finds first string element');

try
{
  expect = '1';
  actual = mixed.find(isString);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, 'mixed: find finds first string element');

try
{
  expect = 1;
  actual = mixed.findIndex(isString);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, 'mixed: findIndex finds first string element');

try
{
  expect = 'sparse';
  actual = sparsestrings.find(isString);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, 'sparsestrings: find finds first string element');

try
{
  expect = 2;
  actual = sparsestrings.findIndex(isString);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, 'sparsestrings: findIndex finds first string element');

try
{
  expect = 'string';
  actual = [].find.call(arraylike, isString);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, 'arraylike: find finds first string element');

try
{
  expect = 1;
  actual = [].findIndex.call(arraylike, isString);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, 'arraylike: findIndex finds first string element');

try
{
  expect = 1;
  actual = 0;
  Array.prototype.find.call({get 0(){ actual++ }, length: 1}, ()=>true);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, 'arraylike with getter: getter only called once');

try
{
  expect = 'string';
  actual = [].find.call(indexedArray, isString);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, 'indexedArray: find finds first string element');

try
{
  expect = 142;
  actual = [].findIndex.call(indexedArray, isString);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, 'indexedArray: findIndex finds first string element');
