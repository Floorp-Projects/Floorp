actual = '';
expected = 'nocrash,';

var v = {};

function a() {
    var o = 3;
    v.f = function() { return o; };
}

for (i = 0; i < 6; i++)
    new a;


appendToActual('nocrash')


assertEq(actual, expected)
