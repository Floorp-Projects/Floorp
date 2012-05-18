function f() {
  var x = 10;
  var g = function(x, Int8Array, arr, f) {
    for (var i = 0; i < 10; ++i) {
      gc();
    }
  }
  for (var i = 0; i < 10; ++i) {
    g(100 * i + x);
  }
}
f();
