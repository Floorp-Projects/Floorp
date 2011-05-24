// |jit-test| debug
function f() { ({}).m = function(){}; }
trap(f, 11, '');
f();
