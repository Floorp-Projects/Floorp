// |jit-test| debug;

// Binary: cache/js-dbg-32-ceef8a5c3ca1-linux
// Flags:
//
function f() { eval(''); }
trap(f, 9, "");
f()
