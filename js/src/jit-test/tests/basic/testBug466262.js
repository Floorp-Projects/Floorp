//test no assert
function testBug466262() {
    var e = 1;
    for (var d = 0; d < 3; ++d) {
      if (d == 2) {
        e = "";
      }
    }
    return true;
}
assertEq(testBug466262(), true);
