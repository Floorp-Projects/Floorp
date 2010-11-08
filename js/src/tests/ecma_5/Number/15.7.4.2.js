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

function testAround(middle)
{
    var range = 260;
    var low = middle - range/2;
    for (var i = 0; i < range; ++i)
        assertEq(low + i, parseInt(String(low + i)));
}

testAround(-Math.pow(2,32));
testAround(-Math.pow(2,16));
testAround(0);
testAround(+Math.pow(2,16));
testAround(+Math.pow(2,32));

reportCompare(true, true);
