// |jit-test| debug
var f = (function () {with ({}) {}});
trap(f, 5, ''); // trap "nullblockchain" op
f();
