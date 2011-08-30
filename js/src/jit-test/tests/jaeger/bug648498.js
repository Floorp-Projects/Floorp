function f(x, y) {
    return x;
}
var a = 3.3;
a ? f(f(1, 2), 3) : a;
