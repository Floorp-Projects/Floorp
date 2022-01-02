
f = function() {
    v = new Uint8Array()
        function f(x) {
            return x + v[0] | 0
        }
    return f
}()
assertEq(f(0), 0)
assertEq(f(1), 0)
