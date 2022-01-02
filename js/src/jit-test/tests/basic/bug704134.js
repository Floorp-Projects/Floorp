function f(s) {
    eval(s);
    return function() {
        with({}) {};
        return b;
    };
}
var b = 1;
var g1 = f("");
var g2 = f("var b = 2;");
g1('');
assertEq(g2(''), 2);
