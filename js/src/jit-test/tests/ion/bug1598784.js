// |jit-test| error:ReferenceError
(function() {
    switch (0) {
        case 0:
            f() = 0;
        case -3:
    }
})();
