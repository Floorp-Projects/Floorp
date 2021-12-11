function zero() { return 0; }
function f(x, a) {
  var test = 0x7fffffff;

  for (var i=0; i<100; i++)
  {
    if (i == 0) {
      test += 1;
      var t = (test > zero()) * (0xffffffff >>> x);
    }
    var test2 = test | 0;
    return [test2,t];
  }
}
var t = f(0, "");
assertEq(t[0], 0x80000000 | 0);
assertEq(t[1], 0xffffffff >>> 0);

var t = f(0);
assertEq(t[0], 0x80000000 | 0);
assertEq(t[1], 0xffffffff >>> 0);
