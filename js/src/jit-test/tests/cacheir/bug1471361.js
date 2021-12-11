// This is a fuzzer-found crash inducing test.
// Pass is simply not crashing.
(function () {
    f = function(y) {
        ~+y;
    }
    x = [new Number];
    for (var j = 0; j < 3; ++j) {
        for (var k = 0; k < 3; ++k) {
            f(x[k]);
        }
    }
})();
