let threw = false;
try {
    ({
        a: arguments.length
    } = 0);
} catch (error) {
    assertEq(error instanceof ReferenceError, true);
    threw = true;
}
assertEq(threw, true);
