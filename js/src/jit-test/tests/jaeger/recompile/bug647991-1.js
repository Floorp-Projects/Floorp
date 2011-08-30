function f() {
    function g() {
        eval("");
        gc();
        Math.abs(4);
        NaN;
    }
    g();
}
function h() {
    var x, y;
    x = Math.floor(-0);
    y = parseInt("1");
}

f();
h();

