
(function(x, x) {
    eval(`
        var y = 1;
        function f() {
            return delete y;
        }
        f();
    `);
})()
