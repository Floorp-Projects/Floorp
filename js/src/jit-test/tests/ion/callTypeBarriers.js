function fixProto(x) {
  x.__proto__ = x.__proto__;
}

function bar() {

  function foo(count) {
    if (count)
      return 10;
    return "foo";
  }
  fixProto(foo);

  function g(x, i) {
    var t;
    for (var j = 0; j < 200; j++)
      t = x(i);
    return t;
  }
  var D;
  for (var i = 0; i < 100; i++)
      D = g(foo, -99 + i);
  return D;
}
print(bar());
