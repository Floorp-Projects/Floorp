function f(x, y) {
  for (var i = 0; i < 100; i++)
    assertEq(x[0], y);
}
var a = new ArrayBuffer(20);
var b = new Int32Array(a, 12, 2);
var c = new Int32Array(a, 0, 2);
b[0] = 10;
f(b, 10);
c[0] = 20;
f(c, 20);
