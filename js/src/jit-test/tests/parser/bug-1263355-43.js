// |jit-test| error: ReferenceError

function f(a = x, x = x) {}
f(/y/)
