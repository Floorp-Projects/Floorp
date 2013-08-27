function f(type) {
    for (var i = 0; i < 3; i++) {}
    assertEq((new String) instanceof type, true);
}
f(String);
