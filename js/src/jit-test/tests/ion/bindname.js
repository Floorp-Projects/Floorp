// Test the scope chain walk.
function test1() {
    var x = 0;
    function f1(addprop) {
        function f2() {
            eval("");
            function f3() {
                eval("");
                function f4() {
                    for (var i=0; i<100; i++) {
                        x = x + i;
                    }
                }
                return f4;
            }
            return f3();
        }
        var g = f2();
        g();
        if (addprop)
            eval("var a1 = 3; var x = 33;");
        g();
        if (addprop)
            assertEq(x, 4983);
        return f2();
    }

    var g = f1(true);
    g();
    g = f1(false);
    eval("var y = 2020; var z = y + 3;");
    g();
    return x;
}
assertEq(test1(), 19800);

// Test with non-cacheable objects on the scope chain.
function test2(o) {
    var x = 0;
    with ({}) {
        with (o) {
            var f = function() {
                for (var i=0; i<100; i++) {
                    x++;
                }
            };
        }
    }
    f();
    assertEq(o.x, 110);
    assertEq(x, 0);
}
test2({x: 10});
