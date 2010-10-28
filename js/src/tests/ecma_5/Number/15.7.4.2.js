/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

assertEq(raisesException(TypeError)('Number.prototype.toString.call(true)'), true);
assertEq(raisesException(TypeError)('Number.prototype.toString.call("")'), true);
assertEq(raisesException(TypeError)('Number.prototype.toString.call({})'), true);
assertEq(raisesException(TypeError)('Number.prototype.toString.call(null)'), true);
assertEq(raisesException(TypeError)('Number.prototype.toString.call([])'), true);
assertEq(raisesException(TypeError)('Number.prototype.toString.call(undefined)'), true);
assertEq(raisesException(TypeError)('Number.prototype.toString.call(new Boolean(true))'), true);

assertEq(completesNormally('Number.prototype.toString.call(42)'), true);
assertEq(completesNormally('Number.prototype.toString.call(new Number(42))'), true);

reportCompare(true, true);
