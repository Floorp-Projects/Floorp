
// Don't use NativeIterator cache for objects with dense elements.

function bar(a) {
  var n = 0;
  for (var b in a) { n++; }
  return n;
}

function foo() {
  var x = {a:0,b:1};
  var y = {a:0,b:1};
  y[0] = 2;
  y[1] = 3;
  for (var i = 0; i < 10; i++) {
    assertEq(bar(x), 2);
    assertEq(bar(y), 4);
  }
}
foo();
