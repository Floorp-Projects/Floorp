// |jit-test| --fast-warmup; --no-threads

function foo() {
  var args = new Float32Array(10);
  return args[0]?.toString();
}

for (var i = 0; i < 50; i++) {
  foo()
}
