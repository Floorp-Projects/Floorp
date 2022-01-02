// Don't eliminate a phi if it has no SSA uses but its value is still
// observable in the interpreter.
var t1 = 100;
function test1(x) {
    // g(x) is inlined with --ion-eager, but don't mark the phi
    // for x as unused.
    for (var i = 0; i < 90; i++) {
        f1(x);
        if (i >= 80)
            t1;
    }
}

function f1(x) {};
test1(2);

var t2 = 100;
function test2(g) {
    // g(x) is inlined with --ion-eager, but don't mark the phi
    // for g as unused.
    for (var i = 0; i < 90; i++) {
        g();
        if (i >= 80)
            t2;
    }
}

function f2() {};
test2(f2);
