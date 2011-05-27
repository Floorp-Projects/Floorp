// |jit-test| debug
function f() { eval(''); }
trap(f, 8, '');
f();
