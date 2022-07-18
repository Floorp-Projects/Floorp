// |jit-test| --fast-warmup

function blackhole() {
  // Prevent Ion from optimising away this function and its arguments.
  with ({});
}

function inner() {
  // Compile-time constants after GVN, but not during scalar replacement.
  // We don't want these to be constants during scalar replacement to ensure
  // we don't optimise away the slice() call.
  const zero = Math.pow(0, 1);
  const one = Math.pow(1, 1);
  const two = Math.pow(2, 1);
  const three = Math.pow(3, 1);
  const four = Math.pow(4, 1);

  // Test with constant |begin| and |count| a constant after GVN.
  {
    let a0 = Array.prototype.slice.call(arguments, 0, zero);
    let a1 = Array.prototype.slice.call(arguments, 0, one);
    let a2 = Array.prototype.slice.call(arguments, 0, two);
    let a3 = Array.prototype.slice.call(arguments, 0, three);
    let a4 = Array.prototype.slice.call(arguments, 0, four);
    blackhole(a0, a1, a2, a3, a4);
  }
  {
    let a0 = Array.prototype.slice.call(arguments, 1, zero);
    let a1 = Array.prototype.slice.call(arguments, 1, one);
    let a2 = Array.prototype.slice.call(arguments, 1, two);
    let a3 = Array.prototype.slice.call(arguments, 1, three);
    let a4 = Array.prototype.slice.call(arguments, 1, four);
    blackhole(a0, a1, a2, a3, a4);
  }

  // Same as above, but this time |begin| isn't a constant during scalar replacement.
  {
    let a0 = Array.prototype.slice.call(arguments, zero, zero);
    let a1 = Array.prototype.slice.call(arguments, zero, one);
    let a2 = Array.prototype.slice.call(arguments, zero, two);
    let a3 = Array.prototype.slice.call(arguments, zero, three);
    let a4 = Array.prototype.slice.call(arguments, zero, four);
    blackhole(a0, a1, a2, a3, a4);
  }
  {
    let a0 = Array.prototype.slice.call(arguments, one, zero);
    let a1 = Array.prototype.slice.call(arguments, one, one);
    let a2 = Array.prototype.slice.call(arguments, one, two);
    let a3 = Array.prototype.slice.call(arguments, one, three);
    let a4 = Array.prototype.slice.call(arguments, one, four);
    blackhole(a0, a1, a2, a3, a4);
  }
}

// Ensure |inner| can be inlined.
assertEq(isSmallFunction(inner), true);

// Zero arguments.
function outer0() {
  trialInline();
  return inner();
}

// One argument.
function outer1() {
  trialInline();
  return inner(1);
}

// Two arguments.
function outer2() {
  trialInline();
  return inner(1, 2);
}

// Three arguments.
function outer3() {
  trialInline();
  return inner(1, 2, 3);
}

// Four arguments. (|inner| can't be inlined anymore!)
function outer4() {
  trialInline();
  return inner(1, 2, 3, 4);
}

// Don't Ion compile the top-level script.
with ({});

for (var i = 0; i < 50; i++) {
  outer0();
  outer1();
  outer2();
  outer3();
  outer4();
}
