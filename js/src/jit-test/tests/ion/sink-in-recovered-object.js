setJitCompilerOption("ion.warmup.trigger", 30);

var arr = [];
function f (cond, a) {
  var obj = { a: 0 };
  var x = 2 * a + 1;
  if (cond) {
    obj.a = x;
    arr.push(obj.a);
    obj.a = 1;
  } else {
    obj.a = 1;
  }
  return obj.a;
}

for (var i = 0; i < 100; i++) {
  assertEq(f(i % 2, i), 1);
}
