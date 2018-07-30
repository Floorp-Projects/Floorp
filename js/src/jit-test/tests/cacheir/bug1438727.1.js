let count = 0;
function testMathyFunction(f, inputs) {
    for (var j = 0; j < inputs.length; ++j)
        for (var k = 0; k < inputs.length; ++k) {
            let a = f(inputs[j], inputs[k]);
            count += 1;
            print("Number " + count + ": " + a + " (inputs "+inputs[j]+","+inputs[k]+")");
        }
}
mathy0 = function(x, y) {
    return (x / (y ? x : -Number.MAX_SAFE_INTEGER) > Math.fround(+y & +0x100000000) ** Math.fround(y))
}
testMathyFunction(mathy0, /*MARR*/ [
    [1], , , [1],
    [1], , [1], , , [1], , [1], , , [1],
    [1], , [1], , , [1],
    [1], , [1], , [1],
    [1],
    [1], , -1 / 0
])

testMathyFunction(mathy0, [, -Number.MAX_VALUE, Number.MIN_SAFE_INTEGER, 0x100000001, 0x07fffffff, -0x07fffffff, 0 / 0, Number.MIN_VALUE, -0x0ffffffff, Number.MAX_SAFE_INTEGER, 0x0ffffffff, -0x100000000, , 1 / 0, 0x080000000, -1 / 0, 0x100000000])