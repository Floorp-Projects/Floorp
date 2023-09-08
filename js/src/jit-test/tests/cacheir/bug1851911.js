// |jit-test| --fast-warmup; --no-threads; --blinterp-eager

function foo(o) {
  return o.x;
}

function bar(n) {
  with ({}) {}
  class C {}
  for (var i = 0; i < n; i++) {
    var c = new C();
    c.x = 0;
    foo(c);
  }
}

with ({}) {}

function triggerIonCompile() {
  for (var i = 0; i < 10; i++) {
    bar(3);
  }
}

triggerIonCompile();

// Fill up shape list
for (var i = 0; i < 6; i++) {
  bar(10);
}

bar(10);
gc();

triggerIonCompile();
