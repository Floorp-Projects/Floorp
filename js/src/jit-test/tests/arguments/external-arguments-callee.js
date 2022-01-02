function foo() {
    with ({}) {}
    return bar(arguments);
}

function bar(x) {
    assertEq(x.callee.name, "foo");
    assertEq(arguments.callee.name, "bar");
}

for (var i = 0; i < 100; i++) {
    foo();
}
