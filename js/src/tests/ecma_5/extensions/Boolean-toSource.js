/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

assertEq(raisesException(TypeError)('Boolean.prototype.toSource.call(42)'), true);
assertEq(raisesException(TypeError)('Boolean.prototype.toSource.call("")'), true);
assertEq(raisesException(TypeError)('Boolean.prototype.toSource.call({})'), true);
assertEq(raisesException(TypeError)('Boolean.prototype.toSource.call(null)'), true);
assertEq(raisesException(TypeError)('Boolean.prototype.toSource.call([])'), true);
assertEq(raisesException(TypeError)('Boolean.prototype.toSource.call(undefined)'), true);
assertEq(raisesException(TypeError)('Boolean.prototype.toSource.call(new String())'), true);

assertEq(completesNormally('Boolean.prototype.toSource.call(true)'), true);
assertEq(completesNormally('Boolean.prototype.toSource.call(new Boolean(true))'), true);

reportCompare(true, true);
