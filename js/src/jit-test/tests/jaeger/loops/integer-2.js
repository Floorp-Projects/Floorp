function foo(x, y, z, a) {
  for (var i = 0x7fff; i < 0xffff; i++) {
    var y = ((x + y) + (z + a[0])) | 0;
  }
  return y;
}
assertEq(foo(0x7fffffff, 0x7fffffff, 0x7fffffff, [0x7fffffff]), 2147385343);

var q = [0x7fffffff];
assertEq(eval("foo(0x7fffffff, 0x7fffffff, {valueOf:function() {q[0] = 'e4'; return 0;}}, q)"), 438048096);
