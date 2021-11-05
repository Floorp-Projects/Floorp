function foo(x,y) {
    delete arguments[0];
    return bar(arguments);
}

function bar(x) {
    return (x[100]|0) + arguments.length;
}

var sum = 0;
for (var i = 0; i < 100; i++) {
    sum += foo(1,2);
}
assertEq(sum, 100);
