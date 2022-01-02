function foo(x,y) {
    delete arguments[1];
    return bar(arguments);
}

function bar(x) {
    return (100 in x) + arguments.length;
}

var sum = 0;
for (var i = 0; i < 100; i++) {
    sum += foo(1,2);
}
assertEq(sum, 100);
