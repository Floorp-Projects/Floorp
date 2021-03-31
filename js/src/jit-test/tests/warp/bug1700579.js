// |jit-test| --fast-warmup; --ion-offthread-compile=off

function foo(y) {
    var a = y - 1;

    if (y) {}

    return bar(a);
}

with ({}) {}
var bar;
function bar1 (x,y) {
    "use strict";
    return x | 0;
}

function bar2(x) {
    return x;
}

bar = bar1;
for (var i = 0; i < 100; i++) {
    foo(1);
}

bar = bar2;
assertEq(foo(-2147483648), -2147483649);
