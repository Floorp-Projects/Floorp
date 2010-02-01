function f() {
  var k = 0;
  
  var g = function() {
    return ++k;
  }

  return g;
}

function h() {
  for (var i = 0; i < 10; ++i) {
    var vf = f();
    assertEq(vf(), 1);
    assertEq(vf(), 2);
    for (var j = 0; j < 10; ++j) {
      assertEq(vf(), j + 3);
    }
  }
}

h();

checkStats({
  recorderAborted: 9,      // Inner tree is trying to grow
});

