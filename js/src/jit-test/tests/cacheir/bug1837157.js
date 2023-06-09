// |jit-test| --fast-warmup; --no-threads
with ({}) {}

function foo(a) {
  a.prop = 0;
}

class A {
  set prop(x) { }
}

function newShape() {
  class B extends A {}
  return new B();
}

function triggerIonCompile() {
  with ({}) {}
  for (var i = 0; i < 50; i++) {
    foo(initialShapes[i % initialShapes.length])
  }
}

var initialShapes = [];
for (var i = 0; i < 8; i++) {
  initialShapes.push(newShape());
}

triggerIonCompile();

for (var i = 0; i < 10; i++) {
  foo(0);
}
foo(newShape());

triggerIonCompile();
