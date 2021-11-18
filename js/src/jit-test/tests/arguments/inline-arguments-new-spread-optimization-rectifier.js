// |jit-test| --fast-warmup

function foo(a,b) {
    with ({}) {}
    return {result: a + b};
}

function inner(x,y) {
    assertEq(x + y, new foo(...arguments).result);
}

function outer0() {
    trialInline();
    inner();
}

function outer1() {
    trialInline();
    inner(1);
}

function outer2() {
    trialInline();
    inner(1, 2);
}

function outer3() {
    trialInline();
    inner(1,2,3);
}

with ({}) {}

for (var i = 0; i < 50; i++) {
    outer0();
    outer1();
    outer2();
    outer3();
}
