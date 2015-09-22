var x;
function f() {
    x = [];
    for (var i = 0; i < 1; ++i) {
        x.push("");
    }
    [0].concat(x);
}
f();
f();
