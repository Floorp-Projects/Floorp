// |jit-test| --fast-warmup
function h(i) {
  if (i === 150) {
      with(this) {} // Don't inline.
      // Trigger an invalidation of f's IonScript (with g inlined into it) on
      // the stack. Before we bail out, replace g and trial-inline the new
      // function. The bailout must not get confused by this.
      gc(this, "shrinking");
      g = (i, x) => x + 20;
      f();
  }
}
function g(i, x) {
  h(i);
  return x + 1;
}
function f() {
  for (var i = 0; i < 300; i++) {
      g(i, "foo");
      g(i, 1);
  }
}
f();
