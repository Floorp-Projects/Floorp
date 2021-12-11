function test(x) {
    for (var i = 0; i < 10; ++i) {
        // Create an IC specialized for lazy arguments when Baseline runs.
        arguments[0];
        // De-optimize lazy arguments by accessing an out-of-bounds argument.
        arguments[10];

        // Overwrite |arguments| to get a Value type.
        arguments = 0;

        for (var j = 0; j < 1500; j++) {}
        return;
    }
}
test(1);
