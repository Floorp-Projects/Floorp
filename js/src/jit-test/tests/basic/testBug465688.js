//test no assert
function testBug465688() {
    for (let d of [-0x80000000, -0x80000000]) - -d;
    return true;
}
assertEq(testBug465688(), true);
