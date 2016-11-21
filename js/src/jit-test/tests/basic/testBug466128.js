// |jit-test| need-for-each

//test no multitrees assert
function testBug466128() {
    for (let a = 0; a < 3; ++a) {
      for each (let b in [1, 2, "three", 4, 5, 6, 7, 8]) {
      }
    }
    return true;
}
assertEq(testBug466128(), true);
