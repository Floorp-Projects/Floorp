function test() {
  var box = { call: function () { return 42.1; } };
  for (var i = 0; i < 50; i++) {
    assertEq(box.call(undefined, 42.1), 42.1);
  }
}
test();
