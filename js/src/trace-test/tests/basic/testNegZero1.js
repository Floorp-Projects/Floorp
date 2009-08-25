function testNegZero1Helper(z) {
    for (let j = 0; j < 5; ++j) { z = -z; }
    return Math.atan2(0, -0) == Math.atan2(0, z);
}

var testNegZero1 = function() { return testNegZero1Helper(0); }
testNegZero1.name = 'testNegZero1';
testNegZero1Helper(1);
assertEq(testNegZero1(), true);
