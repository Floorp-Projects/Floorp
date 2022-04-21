// |jit-test| --fast-warmup


function foo(args) {
  with ({}) {}
  return args.length;
}

function inner() {
  return foo(Array.prototype.slice.call(arguments, 1, 3));
}

function outer0() {
  trialInline();
  return inner();
}

function outer1() {
  trialInline();
  return inner(1);
}

function outer2() {
  trialInline();
  return inner(1, 2);
}

function outer3() {
  trialInline();
  return inner(1, 2, 3)
}

function outer4() {
  trialInline();
  return inner(1, 2, 3, 4)
}

with ({}) {}

for (var i = 0; i < 50; i++) {
  assertEq(outer0(), 0);
  assertEq(outer1(), 0);
  assertEq(outer2(), 1);
  assertEq(outer3(), 2);
  assertEq(outer4(), 2);
}
