// |jit-test| --no-threads; --fast-warmup; --ion-warmup-threshold=0

function a(b = (b, !b)) {
  () => b
}
for (var i = 0; i < 20; i++) {
  a("");
}

try {
  a();
} catch {}

function f(arr) {
  for (var i = 0; i < 10; i++) {
    a(arr.x);
  }
}
f({y: 1, x:1});
f({x:2});
