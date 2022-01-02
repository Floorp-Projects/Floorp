function foo(x,y) {
    return bar(arguments);
}

function bar(x) {
    return baz(...x) + arguments.length;
}

function baz(x,y) {
    return x + y;
}

var sum = 0;
for (var i = 0; i < 100; i++) {
    sum += foo(1,2);
}
assertEq(sum, 400)
