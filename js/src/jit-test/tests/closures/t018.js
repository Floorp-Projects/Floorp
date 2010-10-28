actual = '';
expected = 'undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,';

for (x = 0; x < 10; ++x) {
    for each(let a in ['', NaN]) {
        appendToActual((function() {
            for (let y = 0; y < 1; ++y) {
                '' + a
            }
        })())
    }
}



assertEq(actual, expected)
