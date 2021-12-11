// |jit-test| error: ReferenceError

with(7) {
    function f() {
        if (i == 15) {
            g();
        }
        const x = 42;
        function g() {
            eval("");
            return x;
        }
        return g;
    }
}
for (var i = 0; i < 99; i++) {
    assertEq(f()(), 42);
}
