var N = 100;
var f = Function("return 0" + "+arguments[0]".repeat(N));

for (let i = 0; i < 10000; ++i) {
  assertEq(f(1), N);
}

