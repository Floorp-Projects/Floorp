// |jit-test| debug
function f() { ({}).m = function(){}; }
dis(f);
trap(f, 11, '');
f();
