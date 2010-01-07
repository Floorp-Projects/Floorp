actual = '';
expected = 'nocrash,';

+[(e = {}, (function () e)()) for each (e in ["", {}, "", {}, ""])];

appendToActual('nocrash')


assertEq(actual, expected)
