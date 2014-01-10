/*
* Copyright (c) 2013 Mathias Bynens. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

function assertThrows(fun, errorType) {
  try {
    fun();
    assertEq(true, false, "Expected error, but none was thrown");
  } catch (e) {
    assertEq(e instanceof errorType, true, "Wrong error type thrown");
  }
}

Object.prototype[1] = 2; // try to break `arguments[1]`

assertEq(String.prototype.startsWith.length, 1);
assertEq(String.prototype.propertyIsEnumerable('startsWith'), false);

assertEq('undefined'.startsWith(), true);
assertEq('undefined'.startsWith(undefined), true);
assertEq('undefined'.startsWith(null), false);
assertEq('null'.startsWith(), false);
assertEq('null'.startsWith(undefined), false);
assertEq('null'.startsWith(null), true);

assertEq('abc'.startsWith(), false);
assertEq('abc'.startsWith(''), true);
assertEq('abc'.startsWith('\0'), false);
assertEq('abc'.startsWith('a'), true);
assertEq('abc'.startsWith('b'), false);
assertEq('abc'.startsWith('ab'), true);
assertEq('abc'.startsWith('bc'), false);
assertEq('abc'.startsWith('abc'), true);
assertEq('abc'.startsWith('bcd'), false);
assertEq('abc'.startsWith('abcd'), false);
assertEq('abc'.startsWith('bcde'), false);

assertEq('abc'.startsWith('', NaN), true);
assertEq('abc'.startsWith('\0', NaN), false);
assertEq('abc'.startsWith('a', NaN), true);
assertEq('abc'.startsWith('b', NaN), false);
assertEq('abc'.startsWith('ab', NaN), true);
assertEq('abc'.startsWith('bc', NaN), false);
assertEq('abc'.startsWith('abc', NaN), true);
assertEq('abc'.startsWith('bcd', NaN), false);
assertEq('abc'.startsWith('abcd', NaN), false);
assertEq('abc'.startsWith('bcde', NaN), false);

assertEq('abc'.startsWith('', false), true);
assertEq('abc'.startsWith('\0', false), false);
assertEq('abc'.startsWith('a', false), true);
assertEq('abc'.startsWith('b', false), false);
assertEq('abc'.startsWith('ab', false), true);
assertEq('abc'.startsWith('bc', false), false);
assertEq('abc'.startsWith('abc', false), true);
assertEq('abc'.startsWith('bcd', false), false);
assertEq('abc'.startsWith('abcd', false), false);
assertEq('abc'.startsWith('bcde', false), false);

assertEq('abc'.startsWith('', undefined), true);
assertEq('abc'.startsWith('\0', undefined), false);
assertEq('abc'.startsWith('a', undefined), true);
assertEq('abc'.startsWith('b', undefined), false);
assertEq('abc'.startsWith('ab', undefined), true);
assertEq('abc'.startsWith('bc', undefined), false);
assertEq('abc'.startsWith('abc', undefined), true);
assertEq('abc'.startsWith('bcd', undefined), false);
assertEq('abc'.startsWith('abcd', undefined), false);
assertEq('abc'.startsWith('bcde', undefined), false);

assertEq('abc'.startsWith('', null), true);
assertEq('abc'.startsWith('\0', null), false);
assertEq('abc'.startsWith('a', null), true);
assertEq('abc'.startsWith('b', null), false);
assertEq('abc'.startsWith('ab', null), true);
assertEq('abc'.startsWith('bc', null), false);
assertEq('abc'.startsWith('abc', null), true);
assertEq('abc'.startsWith('bcd', null), false);
assertEq('abc'.startsWith('abcd', null), false);
assertEq('abc'.startsWith('bcde', null), false);

assertEq('abc'.startsWith('', -Infinity), true);
assertEq('abc'.startsWith('\0', -Infinity), false);
assertEq('abc'.startsWith('a', -Infinity), true);
assertEq('abc'.startsWith('b', -Infinity), false);
assertEq('abc'.startsWith('ab', -Infinity), true);
assertEq('abc'.startsWith('bc', -Infinity), false);
assertEq('abc'.startsWith('abc', -Infinity), true);
assertEq('abc'.startsWith('bcd', -Infinity), false);
assertEq('abc'.startsWith('abcd', -Infinity), false);
assertEq('abc'.startsWith('bcde', -Infinity), false);

assertEq('abc'.startsWith('', -1), true);
assertEq('abc'.startsWith('\0', -1), false);
assertEq('abc'.startsWith('a', -1), true);
assertEq('abc'.startsWith('b', -1), false);
assertEq('abc'.startsWith('ab', -1), true);
assertEq('abc'.startsWith('bc', -1), false);
assertEq('abc'.startsWith('abc', -1), true);
assertEq('abc'.startsWith('bcd', -1), false);
assertEq('abc'.startsWith('abcd', -1), false);
assertEq('abc'.startsWith('bcde', -1), false);

assertEq('abc'.startsWith('', -0), true);
assertEq('abc'.startsWith('\0', -0), false);
assertEq('abc'.startsWith('a', -0), true);
assertEq('abc'.startsWith('b', -0), false);
assertEq('abc'.startsWith('ab', -0), true);
assertEq('abc'.startsWith('bc', -0), false);
assertEq('abc'.startsWith('abc', -0), true);
assertEq('abc'.startsWith('bcd', -0), false);
assertEq('abc'.startsWith('abcd', -0), false);
assertEq('abc'.startsWith('bcde', -0), false);

assertEq('abc'.startsWith('', +0), true);
assertEq('abc'.startsWith('\0', +0), false);
assertEq('abc'.startsWith('a', +0), true);
assertEq('abc'.startsWith('b', +0), false);
assertEq('abc'.startsWith('ab', +0), true);
assertEq('abc'.startsWith('bc', +0), false);
assertEq('abc'.startsWith('abc', +0), true);
assertEq('abc'.startsWith('bcd', +0), false);
assertEq('abc'.startsWith('abcd', +0), false);
assertEq('abc'.startsWith('bcde', +0), false);

assertEq('abc'.startsWith('', 1), true);
assertEq('abc'.startsWith('\0', 1), false);
assertEq('abc'.startsWith('a', 1), false);
assertEq('abc'.startsWith('b', 1), true);
assertEq('abc'.startsWith('ab', 1), false);
assertEq('abc'.startsWith('bc', 1), true);
assertEq('abc'.startsWith('abc', 1), false);
assertEq('abc'.startsWith('bcd', 1), false);
assertEq('abc'.startsWith('abcd', 1), false);
assertEq('abc'.startsWith('bcde', 1), false);

assertEq('abc'.startsWith('', +Infinity), true);
assertEq('abc'.startsWith('\0', +Infinity), false);
assertEq('abc'.startsWith('a', +Infinity), false);
assertEq('abc'.startsWith('b', +Infinity), false);
assertEq('abc'.startsWith('ab', +Infinity), false);
assertEq('abc'.startsWith('bc', +Infinity), false);
assertEq('abc'.startsWith('abc', +Infinity), false);
assertEq('abc'.startsWith('bcd', +Infinity), false);
assertEq('abc'.startsWith('abcd', +Infinity), false);
assertEq('abc'.startsWith('bcde', +Infinity), false);

assertEq('abc'.startsWith('', true), true);
assertEq('abc'.startsWith('\0', true), false);
assertEq('abc'.startsWith('a', true), false);
assertEq('abc'.startsWith('b', true), true);
assertEq('abc'.startsWith('ab', true), false);
assertEq('abc'.startsWith('bc', true), true);
assertEq('abc'.startsWith('abc', true), false);
assertEq('abc'.startsWith('bcd', true), false);
assertEq('abc'.startsWith('abcd', true), false);
assertEq('abc'.startsWith('bcde', true), false);

assertEq('abc'.startsWith('', 'x'), true);
assertEq('abc'.startsWith('\0', 'x'), false);
assertEq('abc'.startsWith('a', 'x'), true);
assertEq('abc'.startsWith('b', 'x'), false);
assertEq('abc'.startsWith('ab', 'x'), true);
assertEq('abc'.startsWith('bc', 'x'), false);
assertEq('abc'.startsWith('abc', 'x'), true);
assertEq('abc'.startsWith('bcd', 'x'), false);
assertEq('abc'.startsWith('abcd', 'x'), false);
assertEq('abc'.startsWith('bcde', 'x'), false);

assertEq('[a-z]+(bar)?'.startsWith('[a-z]+'), true);
assertThrows(function() { '[a-z]+(bar)?'.startsWith(/[a-z]+/); }, TypeError);
assertEq('[a-z]+(bar)?'.startsWith('(bar)?', 6), true);
assertThrows(function() { '[a-z]+(bar)?'.startsWith(/(bar)?/); }, TypeError);
assertThrows(function() { '[a-z]+/(bar)?/'.startsWith(/(bar)?/); }, TypeError);
var global = newGlobal();
global.eval('this.re = /(bar)?/');
assertThrows(function() { '[a-z]+/(bar)?/'.startsWith(global.re); }, TypeError);

// http://mathiasbynens.be/notes/javascript-unicode#poo-test
var string = 'I\xF1t\xEBrn\xE2ti\xF4n\xE0liz\xE6ti\xF8n\u2603\uD83D\uDCA9';
assertEq(string.startsWith(''), true);
assertEq(string.startsWith('\xF1t\xEBr'), false);
assertEq(string.startsWith('\xF1t\xEBr', 1), true);
assertEq(string.startsWith('\xE0liz\xE6'), false);
assertEq(string.startsWith('\xE0liz\xE6', 11), true);
assertEq(string.startsWith('\xF8n\u2603\uD83D\uDCA9'), false);
assertEq(string.startsWith('\xF8n\u2603\uD83D\uDCA9', 18), true);
assertEq(string.startsWith('\u2603'), false);
assertEq(string.startsWith('\u2603', 20), true);
assertEq(string.startsWith('\uD83D\uDCA9'), false);
assertEq(string.startsWith('\uD83D\uDCA9', 21), true);

assertThrows(function() { String.prototype.startsWith.call(undefined); }, TypeError);
assertThrows(function() { String.prototype.startsWith.call(undefined, 'b'); }, TypeError);
assertThrows(function() { String.prototype.startsWith.call(undefined, 'b', 4); }, TypeError);
assertThrows(function() { String.prototype.startsWith.call(null); }, TypeError);
assertThrows(function() { String.prototype.startsWith.call(null, 'b'); }, TypeError);
assertThrows(function() { String.prototype.startsWith.call(null, 'b', 4); }, TypeError);
assertEq(String.prototype.startsWith.call(42, '2'), false);
assertEq(String.prototype.startsWith.call(42, '4'), true);
assertEq(String.prototype.startsWith.call(42, 'b', 4), false);
assertEq(String.prototype.startsWith.call(42, '2', 1), true);
assertEq(String.prototype.startsWith.call(42, '2', 4), false);
assertEq(String.prototype.startsWith.call({ 'toString': function() { return 'abc'; } }, 'b', 0), false);
assertEq(String.prototype.startsWith.call({ 'toString': function() { return 'abc'; } }, 'b', 1), true);
assertEq(String.prototype.startsWith.call({ 'toString': function() { return 'abc'; } }, 'b', 2), false);
assertThrows(function() { String.prototype.startsWith.call({ 'toString': function() { throw RangeError(); } }, /./); }, RangeError);
assertThrows(function() { String.prototype.startsWith.call({ 'toString': function() { return 'abc'; } }, /./); }, TypeError);

assertThrows(function() { String.prototype.startsWith.apply(undefined); }, TypeError);
assertThrows(function() { String.prototype.startsWith.apply(undefined, ['b']); }, TypeError);
assertThrows(function() { String.prototype.startsWith.apply(undefined, ['b', 4]); }, TypeError);
assertThrows(function() { String.prototype.startsWith.apply(null); }, TypeError);
assertThrows(function() { String.prototype.startsWith.apply(null, ['b']); }, TypeError);
assertThrows(function() { String.prototype.startsWith.apply(null, ['b', 4]); }, TypeError);
assertEq(String.prototype.startsWith.apply(42, ['2']), false);
assertEq(String.prototype.startsWith.apply(42, ['4']), true);
assertEq(String.prototype.startsWith.apply(42, ['b', 4]), false);
assertEq(String.prototype.startsWith.apply(42, ['2', 1]), true);
assertEq(String.prototype.startsWith.apply(42, ['2', 4]), false);
assertEq(String.prototype.startsWith.apply({ 'toString': function() { return 'abc'; } }, ['b', 0]), false);
assertEq(String.prototype.startsWith.apply({ 'toString': function() { return 'abc'; } }, ['b', 1]), true);
assertEq(String.prototype.startsWith.apply({ 'toString': function() { return 'abc'; } }, ['b', 2]), false);
assertThrows(function() { String.prototype.startsWith.apply({ 'toString': function() { throw RangeError(); } }, [/./]); }, RangeError);
assertThrows(function() { String.prototype.startsWith.apply({ 'toString': function() { return 'abc'; } }, [/./]); }, TypeError);
