var a, b;
function g(x) {
    var y = x++;
    return [x, y];
}
function f() {
    for(var i=0; i<20; i++) {
        [a,b] = g("10");
    }
}
f();
