// 'arguments' in eval

function f() {
    var g = s => eval(s);
    assertEq(g("arguments"), arguments);
}

f();
f(0, 1, 2);
