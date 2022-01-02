actual = '';
expected = '0,0,1,1,2,2,3,3,';

v = 0
{
let f = function() {
    for (let x = 0; x < 4; ++x) {
        v >> x;
        (function() {
            for (let y = 0; y < 2; ++y) {
                appendToActual(x)
            }
        })()
    }
};
(function() {})()
    f(v)
}


assertEq(actual, expected)
