
let w
(function() {
 testgt1 = function(x) {
    return (-0x80000000 >= (x | 0))
}
 testgt2 = function(x) {
    return (+0x7fffffff >= (x | 0))
}
 testgt3 = function(x) {
    return ((x | 0) >= -0x80000000)
}
 testgt4 = function(x) {
    return ((x | 0) >= +0x7fffffff)
}

 testlt1 = function(x) {
    return (-0x80000000 <= (x | 0))
}
 testlt2 = function(x) {
    return (+0x7fffffff <= (x | 0))
}
 testlt3 = function(x) {
    return ((x | 0) <= -0x80000000)
}
 testlt4 = function(x) {
    return ((x | 0) <= +0x7fffffff)
}

})()
assertEq(testgt1(-0x80000000), true);
assertEq(testgt1(-0x80000000), true);
assertEq(testgt1(0), false);
assertEq(testgt2(0x7fffffff), true);
assertEq(testgt2(0x7fffffff), true);
assertEq(testgt2(0), true);
assertEq(testgt3(-0x80000000), true);
assertEq(testgt3(-0x80000000), true);
assertEq(testgt3(0), true);
assertEq(testgt4(0x7fffffff), true);
assertEq(testgt4(0x7fffffff), true);
assertEq(testgt4(0), false);

assertEq(testlt1(-0x80000000), true);
assertEq(testlt1(-0x80000000), true);
assertEq(testlt1(0), true);
assertEq(testlt2(0x7fffffff), true);
assertEq(testlt2(0x7fffffff), true);
assertEq(testlt2(0), false);
assertEq(testlt3(-0x80000000), true);
assertEq(testlt3(-0x80000000), true);
assertEq(testlt3(0), false);
assertEq(testlt4(0x7fffffff), true);
assertEq(testlt4(0x7fffffff), true);
assertEq(testlt4(0), true);
