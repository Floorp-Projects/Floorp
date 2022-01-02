function foo(s) {
    return arguments[s];
}

for (var i = 0; i < 100; i++) {
    assertEq(foo("callee"), foo);
}

for (var i = 0; i < 100; i++) {
    assertEq(foo("length"), 1);
}

for (var i = 0; i < 100; i++) {
    assertEq(foo(0), 0);
}
