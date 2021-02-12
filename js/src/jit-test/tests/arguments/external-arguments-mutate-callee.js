function foo() {
    arguments.callee = {name: "mutated"};
    return bar(arguments);
}

function bar(x) {
    assertEq(x.callee.name, "mutated");
    assertEq(arguments.callee.name, "bar");
}

for (var i = 0; i < 100; i++) {
    foo();
}
