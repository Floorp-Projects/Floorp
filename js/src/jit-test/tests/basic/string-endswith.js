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
assertEq(String.prototype.endsWith.length, 1);
assertEq(String.prototype.propertyIsEnumerable('endsWith'), false);

assertEq('undefined'.endsWith(), true);
assertEq('undefined'.endsWith(undefined), true);
assertEq('undefined'.endsWith(null), false);
assertEq('null'.endsWith(), false);
assertEq('null'.endsWith(undefined), false);
assertEq('null'.endsWith(null), true);

assertEq('abc'.endsWith(), false);
assertEq('abc'.endsWith(''), true);
assertEq('abc'.endsWith('\0'), false);
assertEq('abc'.endsWith('c'), true);
assertEq('abc'.endsWith('b'), false);
assertEq('abc'.endsWith('a'), false);
assertEq('abc'.endsWith('ab'), false);
assertEq('abc'.endsWith('bc'), true);
assertEq('abc'.endsWith('abc'), true);
assertEq('abc'.endsWith('bcd'), false);
assertEq('abc'.endsWith('abcd'), false);
assertEq('abc'.endsWith('bcde'), false);

assertEq('abc'.endsWith('', NaN), true);
assertEq('abc'.endsWith('\0', NaN), false);
assertEq('abc'.endsWith('c', NaN), false);
assertEq('abc'.endsWith('b', NaN), false);
assertEq('abc'.endsWith('a', NaN), false);
assertEq('abc'.endsWith('ab', NaN), false);
assertEq('abc'.endsWith('bc', NaN), false);
assertEq('abc'.endsWith('abc', NaN), false);
assertEq('abc'.endsWith('bcd', NaN), false);
assertEq('abc'.endsWith('abcd', NaN), false);
assertEq('abc'.endsWith('bcde', NaN), false);

assertEq('abc'.endsWith('', false), true);
assertEq('abc'.endsWith('\0', false), false);
assertEq('abc'.endsWith('c', false), false);
assertEq('abc'.endsWith('b', false), false);
assertEq('abc'.endsWith('a', false), false);
assertEq('abc'.endsWith('ab', false), false);
assertEq('abc'.endsWith('bc', false), false);
assertEq('abc'.endsWith('abc', false), false);
assertEq('abc'.endsWith('bcd', false), false);
assertEq('abc'.endsWith('abcd', false), false);
assertEq('abc'.endsWith('bcde', false), false);

assertEq('abc'.endsWith('', undefined), true);
assertEq('abc'.endsWith('\0', undefined), false);
assertEq('abc'.endsWith('c', undefined), true);
assertEq('abc'.endsWith('b', undefined), false);
assertEq('abc'.endsWith('a', undefined), false);
assertEq('abc'.endsWith('ab', undefined), false);
assertEq('abc'.endsWith('bc', undefined), true);
assertEq('abc'.endsWith('abc', undefined), true);
assertEq('abc'.endsWith('bcd', undefined), false);
assertEq('abc'.endsWith('abcd', undefined), false);
assertEq('abc'.endsWith('bcde', undefined), false);

assertEq('abc'.endsWith('', null), true);
assertEq('abc'.endsWith('\0', null), false);
assertEq('abc'.endsWith('c', null), false);
assertEq('abc'.endsWith('b', null), false);
assertEq('abc'.endsWith('a', null), false);
assertEq('abc'.endsWith('ab', null), false);
assertEq('abc'.endsWith('bc', null), false);
assertEq('abc'.endsWith('abc', null), false);
assertEq('abc'.endsWith('bcd', null), false);
assertEq('abc'.endsWith('abcd', null), false);
assertEq('abc'.endsWith('bcde', null), false);

assertEq('abc'.endsWith('', -Infinity), true);
assertEq('abc'.endsWith('\0', -Infinity), false);
assertEq('abc'.endsWith('c', -Infinity), false);
assertEq('abc'.endsWith('b', -Infinity), false);
assertEq('abc'.endsWith('a', -Infinity), false);
assertEq('abc'.endsWith('ab', -Infinity), false);
assertEq('abc'.endsWith('bc', -Infinity), false);
assertEq('abc'.endsWith('abc', -Infinity), false);
assertEq('abc'.endsWith('bcd', -Infinity), false);
assertEq('abc'.endsWith('abcd', -Infinity), false);
assertEq('abc'.endsWith('bcde', -Infinity), false);

assertEq('abc'.endsWith('', -1), true);
assertEq('abc'.endsWith('\0', -1), false);
assertEq('abc'.endsWith('c', -1), false);
assertEq('abc'.endsWith('b', -1), false);
assertEq('abc'.endsWith('a', -1), false);
assertEq('abc'.endsWith('ab', -1), false);
assertEq('abc'.endsWith('bc', -1), false);
assertEq('abc'.endsWith('abc', -1), false);
assertEq('abc'.endsWith('bcd', -1), false);
assertEq('abc'.endsWith('abcd', -1), false);
assertEq('abc'.endsWith('bcde', -1), false);

assertEq('abc'.endsWith('', -0), true);
assertEq('abc'.endsWith('\0', -0), false);
assertEq('abc'.endsWith('c', -0), false);
assertEq('abc'.endsWith('b', -0), false);
assertEq('abc'.endsWith('a', -0), false);
assertEq('abc'.endsWith('ab', -0), false);
assertEq('abc'.endsWith('bc', -0), false);
assertEq('abc'.endsWith('abc', -0), false);
assertEq('abc'.endsWith('bcd', -0), false);
assertEq('abc'.endsWith('abcd', -0), false);
assertEq('abc'.endsWith('bcde', -0), false);

assertEq('abc'.endsWith('', +0), true);
assertEq('abc'.endsWith('\0', +0), false);
assertEq('abc'.endsWith('c', +0), false);
assertEq('abc'.endsWith('b', +0), false);
assertEq('abc'.endsWith('a', +0), false);
assertEq('abc'.endsWith('ab', +0), false);
assertEq('abc'.endsWith('bc', +0), false);
assertEq('abc'.endsWith('abc', +0), false);
assertEq('abc'.endsWith('bcd', +0), false);
assertEq('abc'.endsWith('abcd', +0), false);
assertEq('abc'.endsWith('bcde', +0), false);

assertEq('abc'.endsWith('', 1), true);
assertEq('abc'.endsWith('\0', 1), false);
assertEq('abc'.endsWith('c', 1), false);
assertEq('abc'.endsWith('b', 1), false);
assertEq('abc'.endsWith('ab', 1), false);
assertEq('abc'.endsWith('bc', 1), false);
assertEq('abc'.endsWith('abc', 1), false);
assertEq('abc'.endsWith('bcd', 1), false);
assertEq('abc'.endsWith('abcd', 1), false);
assertEq('abc'.endsWith('bcde', 1), false);

assertEq('abc'.endsWith('', 2), true);
assertEq('abc'.endsWith('\0', 2), false);
assertEq('abc'.endsWith('c', 2), false);
assertEq('abc'.endsWith('b', 2), true);
assertEq('abc'.endsWith('ab', 2), true);
assertEq('abc'.endsWith('bc', 2), false);
assertEq('abc'.endsWith('abc', 2), false);
assertEq('abc'.endsWith('bcd', 2), false);
assertEq('abc'.endsWith('abcd', 2), false);
assertEq('abc'.endsWith('bcde', 2), false);

assertEq('abc'.endsWith('', +Infinity), true);
assertEq('abc'.endsWith('\0', +Infinity), false);
assertEq('abc'.endsWith('c', +Infinity), true);
assertEq('abc'.endsWith('b', +Infinity), false);
assertEq('abc'.endsWith('a', +Infinity), false);
assertEq('abc'.endsWith('ab', +Infinity), false);
assertEq('abc'.endsWith('bc', +Infinity), true);
assertEq('abc'.endsWith('abc', +Infinity), true);
assertEq('abc'.endsWith('bcd', +Infinity), false);
assertEq('abc'.endsWith('abcd', +Infinity), false);
assertEq('abc'.endsWith('bcde', +Infinity), false);

assertEq('abc'.endsWith('', true), true);
assertEq('abc'.endsWith('\0', true), false);
assertEq('abc'.endsWith('c', true), false);
assertEq('abc'.endsWith('b', true), false);
assertEq('abc'.endsWith('ab', true), false);
assertEq('abc'.endsWith('bc', true), false);
assertEq('abc'.endsWith('abc', true), false);
assertEq('abc'.endsWith('bcd', true), false);
assertEq('abc'.endsWith('abcd', true), false);
assertEq('abc'.endsWith('bcde', true), false);

assertEq('abc'.endsWith('', 'x'), true);
assertEq('abc'.endsWith('\0', 'x'), false);
assertEq('abc'.endsWith('c', 'x'), false);
assertEq('abc'.endsWith('b', 'x'), false);
assertEq('abc'.endsWith('a', 'x'), false);
assertEq('abc'.endsWith('ab', 'x'), false);
assertEq('abc'.endsWith('bc', 'x'), false);
assertEq('abc'.endsWith('abc', 'x'), false);
assertEq('abc'.endsWith('bcd', 'x'), false);
assertEq('abc'.endsWith('abcd', 'x'), false);
assertEq('abc'.endsWith('bcde', 'x'), false);

assertEq('[a-z]+(bar)?'.endsWith('(bar)?'), true);
assertThrows(function() { '[a-z]+(bar)?'.endsWith(/(bar)?/); }, TypeError);
assertEq('[a-z]+(bar)?'.endsWith('[a-z]+', 6), true);
assertThrows(function() { '[a-z]+(bar)?'.endsWith(/(bar)?/); }, TypeError);
assertThrows(function() { '[a-z]+/(bar)?/'.endsWith(/(bar)?/); }, TypeError);
var global = newGlobal();
global.eval('this.re = /(bar)?/');
assertThrows(function() { '[a-z]+/(bar)?/'.endsWith(global.re); }, TypeError);

// http://mathiasbynens.be/notes/javascript-unicode#poo-test
var string = 'I\xF1t\xEBrn\xE2ti\xF4n\xE0liz\xE6ti\xF8n\u2603\uD83D\uDCA9';
assertEq(string.endsWith(''), true);
assertEq(string.endsWith('\xF1t\xEBr'), false);
assertEq(string.endsWith('\xF1t\xEBr', 5), true);
assertEq(string.endsWith('\xE0liz\xE6'), false);
assertEq(string.endsWith('\xE0liz\xE6', 16), true);
assertEq(string.endsWith('\xF8n\u2603\uD83D\uDCA9'), true);
assertEq(string.endsWith('\xF8n\u2603\uD83D\uDCA9', 23), true);
assertEq(string.endsWith('\u2603'), false);
assertEq(string.endsWith('\u2603', 21), true);
assertEq(string.endsWith('\uD83D\uDCA9'), true);
assertEq(string.endsWith('\uD83D\uDCA9', 23), true);

assertThrows(function() { String.prototype.endsWith.call(undefined); }, TypeError);
assertThrows(function() { String.prototype.endsWith.call(undefined, 'b'); }, TypeError);
assertThrows(function() { String.prototype.endsWith.call(undefined, 'b', 4); }, TypeError);
assertThrows(function() { String.prototype.endsWith.call(null); }, TypeError);
assertThrows(function() { String.prototype.endsWith.call(null, 'b'); }, TypeError);
assertThrows(function() { String.prototype.endsWith.call(null, 'b', 4); }, TypeError);
assertEq(String.prototype.endsWith.call(42, '2'), true);
assertEq(String.prototype.endsWith.call(42, '4'), false);
assertEq(String.prototype.endsWith.call(42, 'b', 4), false);
assertEq(String.prototype.endsWith.call(42, '2', 1), false);
assertEq(String.prototype.endsWith.call(42, '2', 4), true);
assertEq(String.prototype.endsWith.call({ 'toString': function() { return 'abc'; } }, 'b', 0), false);
assertEq(String.prototype.endsWith.call({ 'toString': function() { return 'abc'; } }, 'b', 1), false);
assertEq(String.prototype.endsWith.call({ 'toString': function() { return 'abc'; } }, 'b', 2), true);
assertThrows(function() { String.prototype.endsWith.call({ 'toString': function() { throw RangeError(); } }, /./); }, RangeError);
assertThrows(function() { String.prototype.endsWith.call({ 'toString': function() { return 'abc' } }, /./); }, TypeError);

assertThrows(function() { String.prototype.endsWith.apply(undefined); }, TypeError);
assertThrows(function() { String.prototype.endsWith.apply(undefined, ['b']); }, TypeError);
assertThrows(function() { String.prototype.endsWith.apply(undefined, ['b', 4]); }, TypeError);
assertThrows(function() { String.prototype.endsWith.apply(null); }, TypeError);
assertThrows(function() { String.prototype.endsWith.apply(null, ['b']); }, TypeError);
assertThrows(function() { String.prototype.endsWith.apply(null, ['b', 4]); }, TypeError);
assertEq(String.prototype.endsWith.apply(42, ['2']), true);
assertEq(String.prototype.endsWith.apply(42, ['4']), false);
assertEq(String.prototype.endsWith.apply(42, ['b', 4]), false);
assertEq(String.prototype.endsWith.apply(42, ['2', 1]), false);
assertEq(String.prototype.endsWith.apply(42, ['2', 4]), true);
assertEq(String.prototype.endsWith.apply({ 'toString': function() { return 'abc'; } }, ['b', 0]), false);
assertEq(String.prototype.endsWith.apply({ 'toString': function() { return 'abc'; } }, ['b', 1]), false);
assertEq(String.prototype.endsWith.apply({ 'toString': function() { return 'abc'; } }, ['b', 2]), true);
assertThrows(function() { String.prototype.endsWith.apply({ 'toString': function() { throw RangeError(); } }, [/./]); }, RangeError);
assertThrows(function() { String.prototype.endsWith.apply({ 'toString': function() { return 'abc' } }, [/./]); }, TypeError);
