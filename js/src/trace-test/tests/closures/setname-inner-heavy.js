actual = '';
expected = 'undefined,';

function f() { 
    (eval("\
        function () {\
            for (var z = 0; z < 2; ++z) {\
                x = ''\
            }\
        }\
    "))();
}
__defineSetter__("x", eval)
f()
appendToActual(x);


assertEq(actual, expected)
