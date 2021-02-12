function escape(x) { with ({}) {} }

function foo() {
    escape(arguments);
    return bar.apply({}, arguments);
}

function bar(x,y) {
    return x + y;
}

with ({}) {}

var sum = 0;
for (var i = 0; i < 100; i++) {
    sum += foo(1,2);
}
assertEq(sum, 300);
