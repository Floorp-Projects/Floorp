// Verify that we throw when calling/constructing functions that must be
// constructed/called.

function foo(f) {
  f();
}

function new_foo(f) {
  new f();
}

with ({}) {}

function f1() {}
function f2() {}

// Ion-compile the generic trampoline.
for (var i = 0; i < 1000; i++) {
  foo(f1);
  foo(f2);
  new_foo(f1);
  new_foo(f2);
}

class A {}
class B extends A {}
var caught_constructor = false;
try {
  foo(B);
} catch {
  caught_constructor = true;
}
assertEq(caught_constructor, true);

var caught_non_constructor = false;
try {
  new_foo(() => {});
} catch {
  caught_non_constructor = true;
}
assertEq(caught_non_constructor, true);
