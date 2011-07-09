// |jit-test| debug
function f() { eval(''); }
trap(f, 6, '');
f();
