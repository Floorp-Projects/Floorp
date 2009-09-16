var d = 1;
function heavy(x) {
    eval(x);
    return function lite() {
        var s = 0;
        for (var i = 0; i < 9; i++)
            s += d;
        return s;
    };
}

var f1 = heavy("1");
var f2 = heavy("var d = 100;");
assertEq(f1(), 9);
assertEq(f2(), 900);
