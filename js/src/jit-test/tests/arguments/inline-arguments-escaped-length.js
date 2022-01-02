// |jit-test| --fast-warmup

var arg = 0;

function inner() {
    return arguments;
}

function outer0() {
    trialInline();
    return inner().length;
}

function outer1() {
    trialInline();
    return inner(arg).length;
}

function outer2() {
    trialInline();
    return inner(arg, arg).length;
}

function outer3() {
    trialInline();
    return inner(arg, arg, arg).length;
}

function outer4() {
    trialInline();
    return inner(arg, arg, arg, arg).length;
}

with ({}) {}

for (var i = 0; i < 50; i++) {
    assertEq(outer0(), 0);
    assertEq(outer1(), 1);
    assertEq(outer2(), 2);
    assertEq(outer3(), 3);
    assertEq(outer4(), 4);
}
