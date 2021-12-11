let g = newGlobal({immutablePrototype: false});
g.__proto__ = {};
g.evaluate(`(function() {
  for (var i = 0; i < 5; i++) {
    d = eval('1');
    assertEq(d, 1);
  }
})()`);
