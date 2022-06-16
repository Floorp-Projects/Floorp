/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 1704385;
var summary = 'Array.prototype.findLast and Array.prototype.findLastIndex';

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

// findLast and findLastIndex have 1 required argument

expect = 1;
actual = Array.prototype.findLast.length;
reportCompare(expect, actual, 'Array.prototype.findLast.length == 1');
actual = Array.prototype.findLastIndex.length;
reportCompare(expect, actual, 'Array.prototype.findLastIndex.length == 1');

// throw TypeError if no predicate specified
expect = 'TypeError';
try
{
  strings.findLast();
  actual = 'no error';
}
catch(e)
{
  actual = e.name;
}
reportCompare(expect, actual, 'Array.findLast(undefined) throws TypeError');
try
{
  strings.findLastIndex();
  actual = 'no error';
}
catch(e)
{
  actual = e.name;
}
reportCompare(expect, actual, 'Array.findLastIndex(undefined) throws TypeError');

// Length gets treated as integer, not uint32
obj = { length: -4294967295, 0: 42 };
expected = undefined;
actual = Array.prototype.findLast.call(obj, () => true);
reportCompare(expected, actual, 'findLast correctly treats "length" as an integer');
expected = -1
actual = Array.prototype.findLastIndex.call(obj, () => true);
reportCompare(expected, actual, 'findLastIndex correctly treats "length" as an integer');

// test findLast and findLastIndex results
try
{
  expect = 'WORLD';
  actual = strings.findLast(isString);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, 'strings: findLast finds last string element');

try
{
  expect = 2;
  actual = strings.findLastIndex(isString);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, 'strings: findLastIndex finds last string element');

try
{
  expect = '1';
  actual = mixed.findLast(isString);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, 'mixed: findLast finds last string element');

try
{
  expect = 1;
  actual = mixed.findLastIndex(isString);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, 'mixed: findLastIndex finds last string element');

try
{
  expect = 'sparse';
  actual = sparsestrings.findLast(isString);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, 'sparsestrings: findLast finds last string element');

try
{
  expect = 2;
  actual = sparsestrings.findLastIndex(isString);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, 'sparsestrings: findLastIndex finds first string element');

try
{
  expect = 'string';
  actual = [].findLast.call(arraylike, isString);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, 'arraylike: findLast finds last string element');

try
{
  expect = 1;
  actual = [].findLastIndex.call(arraylike, isString);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, 'arraylike: findLastIndex finds last string element');

try
{
  expect = 1;
  actual = 0;
  Array.prototype.findLast.call({get 0(){ actual++ }, length: 1}, ()=>true);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, 'arraylike with getter: getter only called once');

try
{
  expect = 'string';
  actual = [].findLast.call(indexedArray, isString);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, 'indexedArray: findLast finds last string element');

try
{
  expect = 142;
  actual = [].findLastIndex.call(indexedArray, isString);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, 'indexedArray: findLastIndex finds last string element');

// Bug 1058394 - Array#findLast and Array#findLastIndex no longer skip holes too.
var sparseArray = [1,,];
var sparseArrayWithInheritedDataProperty = Object.setPrototypeOf([1,,], {
  __proto__: [].__proto__,
  2 : 0
});
var sparseArrayWithInheritedAccessorProperty = Object.setPrototypeOf([1,,], {
  __proto__: [].__proto__,
  get 2(){
    throw "get 2";
  }
});

try
{
  expect = undefined;
  actual = sparseArray.findLast(() => true);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, "Don't skip holes in Array#findLast.");

try
{
  expect = 0;
  actual = sparseArray.findLastIndex(() => true);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, "Don't skip holes in Array#findLastIndex.");

try
{
  expect = 0;
  actual = sparseArrayWithInheritedDataProperty.findLast(v => v === 0);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, "Array#findLast can find inherited data property.");

try
{
  expect = 2;
  actual = sparseArrayWithInheritedDataProperty.findLastIndex(v => v === 0);
}
catch(e)
{
  actual = dumpError(e);
}
reportCompare(expect, actual, "Array#findLastIndex can find inherited data property.");

try
{
  expect = "get 2";
  actual = sparseArrayWithInheritedAccessorProperty.findLast(() => true);
}
catch(e)
{
  actual = e;
}
reportCompare(expect, actual, "Array#findLast can find inherited accessor property.");

try
{
  expect = "get 2";
  actual = sparseArrayWithInheritedAccessorProperty.findLastIndex(() => true);
}
catch(e)
{
  actual = e;
}
reportCompare(expect, actual, "Array#findLastIndex can find inherited accessor property.");
