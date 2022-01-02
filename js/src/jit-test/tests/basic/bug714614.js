function testForVarInWith(foo, foo) {
  return eval("with ({}) { for (var x = 0; x < 5; x++); } (function() { return delete x; })");
}
f = testForVarInWith()();

