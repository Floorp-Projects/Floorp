if (!('oomTest' in this))
    quit();

oomTest(() => {
    try {
        var max = 400;
        function f(b) {
            if (b) {
                f(b - 1);
            } else {
                g = {};
            }
            g.apply(null, arguments);
        }
        f(max - 1);
    } catch(exc0) {}
    f();
});
