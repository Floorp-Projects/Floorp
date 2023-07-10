// Verify that the generic call trampoline handles fun_call with
// non-object / non-callable `this` correctly

function foo(f) {
  f.call({});
}

with ({}) {}

function f1() {}
function f2() {}

// Ion-compile the generic trampoline.
for (var i = 0; i < 1000; i++) {
  foo(f1, {});
  foo(f2, {});
}

function assertThrows(f) {
  var caught = false;
  try {
    foo(f);
  } catch {
    caught = true;
  }
  assertEq(caught, true);
}

// Non-object callee
assertThrows(0);
assertThrows(undefined);
assertThrows("");

// Non-callable object callee
assertThrows({});
