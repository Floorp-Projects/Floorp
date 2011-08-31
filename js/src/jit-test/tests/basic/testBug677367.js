// |jit-test| debug

function f() {}
trap(f, 0, 'eval("2+2")');
f();
