function inner() {
    return arguments.callee;
}
function inner_escaped() {
    return arguments;
}
function outer() {
    assertEq(inner(), inner);
    assertEq(inner_escaped().callee, inner_escaped);
    assertEq(arguments.callee, outer);
}

with({}) {}
for (var i = 0; i < 100; i++) {
    outer();
}
