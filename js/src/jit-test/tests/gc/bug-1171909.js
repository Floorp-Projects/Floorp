// |jit-test| skip-if: !('oomTest' in this)

oomTest((function(x) { assertEq(x + y + ex, 25); }));
