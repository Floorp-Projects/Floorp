
function testLocalNames() {
    var NaN = 4;
    var undefined = 5;
    var Infinity = 6;
    return NaN + undefined + Infinity;
}
assertEq(testLocalNames(), 15);

