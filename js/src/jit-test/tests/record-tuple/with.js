// |jit-test| skip-if: !this.hasOwnProperty("Tuple")

function f() {
    var expected = #[1, "monkeys", 3];
    assertEq(#[1,2,3].with(1, "monkeys"), expected);
    assertEq(Object(#[1,2,3]).with(1, "monkeys"), expected);
}

for (i = 0; i < 500; i++) {
    f();
}
