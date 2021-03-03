function exists() {
  var a = {'null': 0, 'undefined': 0};
  for (var i = 0; i < 100; i++) {
    assertEq(null in a, true);
    assertEq(undefined in a, true);
  }
}

function missing() {
  var a = {};
  for (var i = 0; i < 100; i++) {
    assertEq(null in a, false);
    assertEq(undefined in a, false);
  }
}

function mixed() {
  var x = [{'null': 0}, {'undefined': 0}]
  for (var i = 0; i < 100; i++) {
    var a = x[i % 2];
    assertEq(null in a, i % 2 == 0);
    assertEq(undefined in a, i % 2 == 1);
  }
}

exists();
missing();
mixed();
