  // |jit-test| --fast-warmup; --no-threads

function main() {
  // Disable Warp compilation, so we don't inline |f|.
  with ({}) {}

  let begin = 0;
  for (let i = 1; i < 30; i++) {
    f(begin);
    begin = undefined;
  }
}
main();

function g(i) {
  return i < 3;
}

function f(begin) {
  // Loop body is only reachable on the first invocation.
  for (let i = begin; i < 5; i++) {
    // |arguments| with out-of-bounds access. This adds a guard on the prototype
    // of the arguments object.
    arguments[100];

    // Loop with a call expression. This ensures we emit bail instructions for
    // unreachable code after the first invocation.
    for (let j = 0; g(j); j++) {}

    // Change the prototype of the arguments object. This will cause a failure
    // on the prototype guard added above.
    arguments.__proto__ = {};
  }
}
