function foo(y) {
    return (1 >>> y >> 12) % 1.5 >>> 0
}
function bar(y) {
    return (1 >>> y >> 12) / 1.5 >>> 0
}

with ({}) {}
for (var i = 0; i < 100; i++) {
    foo(1);
    bar(1);
}
