var Q = 0;
try {
   (function f(i) { Q = i; if (i == 100000) return; f(i+1); })(1)
} catch (e) {
}

// Exact behavior of recursion check depends on which JIT we use.
var ok = (Q == 3000 || Q == 3001);
assertEq(ok, true);

