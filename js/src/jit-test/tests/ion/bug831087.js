function isNegZero(x) {
  return x===0 && (1/x)===-Infinity;
}

try {
    for (y = 0; y < 1; y++) {
        x = y;
    }
} catch (e) {}

function f() {
    (x /= -9)
}
f()
assertEq(isNegZero(this.x), true);
