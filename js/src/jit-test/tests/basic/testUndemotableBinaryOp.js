function testUndemotableBinaryOp() {
    var out = [];
    for (let j = 0; j < 5; ++j) { out.push(6 - ((void 0) ^ 0x80000005)); }
    return out.join(",");
}
assertEq(testUndemotableBinaryOp(), "2147483649,2147483649,2147483649,2147483649,2147483649");
