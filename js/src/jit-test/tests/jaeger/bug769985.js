
/* Make sure ints are converted to doubles as needed at the head of loops which modify those ints. */

function bar() {}

function foo() {
  var n = 0.5;
  n -= 0.5;
  var iters = 0;
  for (var i = n;; ) {
    bar();
    iters++;
    if (i == 100)
      break;
    i = (i + 1) | 0;
  }
  assertEq(iters, 101);
}
foo();
