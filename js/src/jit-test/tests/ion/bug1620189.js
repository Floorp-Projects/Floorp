function test(...a) {
    // Overwrites the parameter |a|, but otherwise isn't used within this
    // function, which should allow to optimise away the function expression.
    // On bailout, this instruction can be safely repeated.
    function a() {}

    // Read an element from the implicit |arguments| binding to ensure the
    // arguments object gets created.
    assertEq(arguments[0], 0);
}
test(0);