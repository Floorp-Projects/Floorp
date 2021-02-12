function foo(x,y) {
    with ({}) {}
    return bar(arguments);
}

function bar(x) {
    return x.length + arguments.length;
}

var sum = 0;
for (var i = 0; i < 100; i++) {
    sum += foo(1,2);
}
assertEq(sum, 300);
