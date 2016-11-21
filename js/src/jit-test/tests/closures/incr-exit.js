// |jit-test| need-for-each

actual = '';
expected = 'ddd,';

// Bug 508187
{ let f = function (y) {
    { let ff = function (g) {
        for each(let h in g) {
            if (++y > 5) {
	        appendToActual('ddd')
            }
        }
    };
      ff(['', null, '', false, '', '', null])
    }
};
    f(-1)
}


assertEq(actual, expected)
