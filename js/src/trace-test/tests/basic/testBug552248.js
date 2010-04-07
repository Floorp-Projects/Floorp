var a = new Array();

function i(save) {
    var x = 9;
    evalInFrame(0, "a.push(x)", save);
    evalInFrame(1, "a.push(z)", save);
    evalInFrame(2, "a.push(z)", save);
    evalInFrame(3, "a.push(y)", save);
    evalInFrame(4, "a.push(x)", save);
}

function h() {
    var z = 5;
    evalInFrame(0, "a.push(z)");
    evalInFrame(1, "a.push(y)");
    evalInFrame(2, "a.push(x)");
    evalInFrame(0, "i(false)");
    evalInFrame(0, "a.push(z)", true);
    evalInFrame(1, "a.push(y)", true);
    evalInFrame(2, "a.push(x)", true);
    evalInFrame(0, "i(true)", true);
}

function g() {
    var y = 4;
    h();
}

function f() {
    var x = 3;
    g();
}

f();
assertEq(a+'', [5, 4, 3, 9, 5, 5, 4, 3, 5, 4, 3, 9, 5, 5, 4, 3]+'');
