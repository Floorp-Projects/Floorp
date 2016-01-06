if (!('oomTest' in this))
  quit();

oomTest(() => {
    var max = 400;
    function f(b) {
        if (b) {
            f(b - 1);
        } else eval('"use strict"; const z = w; z = 1 + w; c = 5');
    }
    f(max - 1);
});
