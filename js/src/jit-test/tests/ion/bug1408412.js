function g() {
    return arguments.length;
}
function f() {
    with(this) {};
    for (var i = 0; i < 100; i++) {
        g();
    }
    var s = "for (var j = 0; j < 1200; j++) assertEq(g(";
    for (var i = 0; i < 5000; i++)
        s += i + ",";
    s += "1), 5001);";
    eval(s);
}
f();
