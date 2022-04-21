// |jit-test| --fast-warmup

function foo(args) {
  with ({}) {}
  return args.length;
}

function inner() {
  return arguments;
}

function outer0() {
  trialInline();
  var args = Array.prototype.slice.call(inner(), 1, 3);
  return foo(args);
}

function outer1() {
  trialInline();
  var args = Array.prototype.slice.call(inner(1), 1, 3);
  return foo(args);
}

function outer2() {
  trialInline();
  var args = Array.prototype.slice.call(inner(1, 2), 1, 3);
  return foo(args);
}

function outer3() {
  trialInline();
  var args = Array.prototype.slice.call(inner(1, 2, 3), 1, 3);
  return foo(args);
}

function outer4() {
  trialInline();
  var args = Array.prototype.slice.call(inner(1, 2, 3, 4), 1, 3);
  return foo(args);
}

with ({}) {}

for (var i = 0; i < 50; i++) {
  assertEq(outer0(), 0);
  assertEq(outer1(), 0);
  assertEq(outer2(), 1);
  assertEq(outer3(), 2);
  assertEq(outer4(), 2);
}
