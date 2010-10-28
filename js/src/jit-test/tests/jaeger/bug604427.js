
function testInt(x) {
    var a = x|0;
    return (a !== a);
}
assertEq(testInt(10), false);
