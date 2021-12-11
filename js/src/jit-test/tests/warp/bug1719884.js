// |jit-test| --fast-warmup; --no-threads

var always_true = true;
var unused = 3;

function foo(input, trigger) {
  if (trigger) {
    return Math.atan2(always_true ? Math.trunc(input ** -0x80000001)
                                  : unused,
                      +input);
  }
}

with ({}) {}
for (var i = 0; i < 100; i++) {
    foo(1, i == 15);
}

assertEq(foo(-Infinity, true), -Math.PI);
