actual = '';
expected = '0,1,2,0,1,2,';

for (var a = 0; a < 2; ++a) {
    for (var b = 0; b < 3; ++b) {
        (function (x) {
            (function () {
                for (var c = 0; c < 1; ++c) {
                    appendToActual(x);
                }
            })();
        })(b);
    }
}


assertEq(actual, expected)
