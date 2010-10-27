actual = '';
expected = 'undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,undefined,';

for (x = 0; x < 10; ++x) {
  for each(let a in ['', NaN, 3, 5.5, {}]) {
        appendToActual((function() {
            for (let y = 0; y < 1; ++y) {
                '' + a
            }
        })())
    }
}



assertEq(actual, expected)
