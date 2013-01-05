function g() { }
function f(b) {
    var test;
    if (b) {
        g.apply(null, arguments);
        var test = 1;
    } else {
        f(false);
    }
}
f(true);
