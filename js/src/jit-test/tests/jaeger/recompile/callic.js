
/* Recompilation while being processed by a call IC. */

var g;
function foo() {
  for (g = 0; g < 5; g++) {
    bar();
  }
  function bar() {
    with ({}) {
      eval("g = undefined;");
    }
  }
}
foo();

assertEq(g, NaN);
