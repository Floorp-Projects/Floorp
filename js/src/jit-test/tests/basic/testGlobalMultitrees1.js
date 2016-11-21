// |jit-test| need-for-each

//test no multitrees assert
function testGlobalMultitrees1() {
    (function() { 
      for (var j = 0; j < 4; ++j) {
        for each (e in ['A', 1, 'A']) {
        }
      }
    })();
    return true;
}
assertEq(testGlobalMultitrees1(), true);
