actual = '';
expected = '787878,';

var q = [];
for (var a = 0; a < 3; ++a) {
    (function () {
        for (var b = 7; b < 9; ++b) {
            (function () {
                for (var c = 0; c < 1; ++c) {
		  q.push(b);
                }
            })();
        }
    })();
}
appendToActual(q.join(""));


assertEq(actual, expected)
