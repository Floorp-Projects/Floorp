// |jit-test| --fast-warmup; --no-threads; --blinterp-eager

function foo(o) {
  return foo_inner(o);
}

function foo_inner(o) {
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

// Overflow shape list, adding a new baseline IC.
bar(10);

// Purge a stub inside a monomorphically-inlined script.
gc();

triggerIonCompile();
