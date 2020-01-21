var g = newGlobal({deferredParserAlloc: true});
function test() {
  var x = /hi/;
  var y = 128n;
  function inner(a, b) {
    return a + b;
  }
  var a = inner(y, y);
  return x.exec(a.toString());
}
g.evaluate(test.toString());
assertEq(g.evaluate('test()'), null);
