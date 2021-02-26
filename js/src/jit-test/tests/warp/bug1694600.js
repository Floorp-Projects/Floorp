function foo(fn, y) {
    var dummy = y > 0 ? 1 : 2;
    fn();
    return y * y;
}

function nop() {}
function throws() { throw 1; }

with ({}) {}
for (var i = 0; i < 100; i++) {
    foo(nop, 0)
    try {
	foo(throws, 0x7fffffff)
    } catch {}
}
