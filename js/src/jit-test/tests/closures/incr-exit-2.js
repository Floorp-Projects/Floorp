// |jit-test| need-for-each

actual = '';
expected = '-3,';

v = 0
{ let f = function (y) {
    { let f = function (g) {
        for each(let h in g) {
            if (++y > 2) {
                appendToActual(h)
            }
        }
    };
        f([(--y), false, true, (--y), false, (--y)])
    }
};
    f(v)
}


assertEq(actual, expected)
