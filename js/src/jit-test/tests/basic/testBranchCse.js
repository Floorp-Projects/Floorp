function testBranchCse() {
    empty = [];
    out = [];
    for (var j=0;j<10;++j) { empty[42]; out.push((1 * (1)) | ""); }
    return out.join(",");
}
assertEq(testBranchCse(), "1,1,1,1,1,1,1,1,1,1");
