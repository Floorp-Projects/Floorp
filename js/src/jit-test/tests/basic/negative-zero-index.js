function test() {
    const array = [1];
    for (let i = 0; i < 10; i++) {
        assertEq(array[0], 1);
        assertEq(array[0.0], 1);
        assertEq(array[-0.0], 1);
        // ToPropertyKey(-0.0) is "0", but "-0" is distinct!
        assertEq(array["-0"], undefined);
    }

    const string = "a";
    for (let i = 0; i < 10; i++) {
        assertEq(string[0], "a");
        assertEq(string[0.0], "a");
        assertEq(string[-0.0], "a");
        assertEq(string["-0"], undefined);
    }
}

test();
