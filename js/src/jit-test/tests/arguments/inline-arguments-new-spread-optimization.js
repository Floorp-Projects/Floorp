// |jit-test| --fast-warmup


function foo(...args) {
    with ({}) {}
    return {result: args.length};
}

function inner() {
    // Single argument spread calls with |arguments| can be optimised.
    return new foo(...arguments);
}

function outer0() {
    trialInline();
    return inner();
}

function outer1() {
    trialInline();
    return inner(1);
}

function outer2() {
    trialInline();
    return inner(1, 2);
}

function outer3() {
    trialInline();
    return inner(1,2,3)
}

function outer4() {
    trialInline();
    return inner(1,2,3,4)
}

with ({}) {}

for (var i = 0; i < 50; i++) {
    assertEq(outer0().result, 0);
    assertEq(outer1().result, 1);
    assertEq(outer2().result, 2);
    assertEq(outer3().result, 3);
    assertEq(outer4().result, 4);
}
