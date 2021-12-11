// |jit-test| error: TypeError
(function () {
    var i = 0; 
    (function () {
        var x;
        (x = "3") || 1;
        (x = "")(i || x);
    })();
})();
