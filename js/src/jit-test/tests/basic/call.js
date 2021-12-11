function glob_f1() {
  return 1;
}
function glob_f2() {
  return glob_f1();
}
function call()
{
  var q1 = 0, q2 = 0, q3 = 0, q4 = 0, q5 = 0;
  var o = {};
  function f1() {
      return 1;
  }
  function f2(f) {
      return f();
  }
  o.f = f1;
  for (var i = 0; i < 100; ++i) {
      q1 += f1();
      q2 += f2(f1);
      q3 += glob_f1();
      q4 += o.f();
      q5 += glob_f2();
  }
  var ret = String([q1, q2, q3, q4, q5]);
  return ret;
}
assertEq(call(), "100,100,100,100,100");
