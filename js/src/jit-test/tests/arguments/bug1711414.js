function bar() { this[0] = "overwritten"; }

function foo() {
    bar.apply(arguments, arguments);
    return arguments[0];
}

with ({}) {}
for (var i = 0; i < 100; i++) {
    assertEq(foo("original"), "overwritten");
}
