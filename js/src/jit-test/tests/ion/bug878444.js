function g()
{
    function f(v) {
        v = +v;
        var infinity = 0.0;
        var three = 0.0;
        var nan = 0.;
        var result = 0;

        infinity = 1.0 / 0.0;
        three = v + 2.0;
        nan = (infinity % three);
        result = ~~(nan + 42.0);

        return result | 0;
    }
    return f
}

g = g()
var x;
for(i=0; i < 20000; ++i)
{
    x = g(1.0)
}
assertEq(x, 0);
