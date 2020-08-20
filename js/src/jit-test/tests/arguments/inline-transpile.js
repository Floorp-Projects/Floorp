function foo(i) {
    return i;
}

function bar(n) {
    return foo.apply({}, arguments);
}

function baz(a, n) {
    return bar(n);
}

var sum = 0;
for (var i = 0; i < 10000; i++) {
    sum += baz(0, 1);
}
assertEq(sum, 10000);
