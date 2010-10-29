function testNestedClosures() {
    function f(a, b) {
        function g(x, y) {
            function h(m, n) {
                function k(u, v) {
                    var s = '';
                    for (var i = 0; i < 5; ++i)
                        s = a + ',' + b + ',' + x + ',' + y + ',' + m + ',' + n + ',' + u + ',' + v;
                    return s;
                }
                return k(m+1, n+1);
            }
            return h(x+1, y+1);
        }
        return g(a+1, b+1);
    }

    var s1;
    for (var i = 0; i < 5; ++i)
        s1 = f(i, i+i);
    return s1;
}
assertEq(testNestedClosures(), '4,8,5,9,6,10,7,11');
