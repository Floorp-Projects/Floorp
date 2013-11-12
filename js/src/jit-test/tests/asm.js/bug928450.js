(function() {
    "use asm";
    function f() {
        i((1.5 != 2.) ? 3 : 0);
    }
    return f;
})();

// Bug 933104
(function() {
    "use asm";
    function f(x) {
        x = +x;
        x = -2.;
        (x > -1.5) ? 0 : 0;
    }
    return f;
})()
