// |jit-test| debug
var f = (function () {with ({}) {}});
dis(f);
trap(f, 5, ''); // trap "nullblockchain" op
f();
