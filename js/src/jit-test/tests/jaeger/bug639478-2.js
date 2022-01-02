var y;
function f() {
    for(var _ in [3.14]) {
        y = 3.14;
        y = y ^ y;
        return y;
        
        function g() {
        }
    }
}
assertEq(f(), 0);
