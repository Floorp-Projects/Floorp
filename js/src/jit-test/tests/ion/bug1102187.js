
function minmax() {
    // The test cases for minmax with two operands.
    // Test integer type.
    var pair_min = Math.min(1, 2);
    assertEq(pair_min, 1);
    var pair_max = Math.max(1, 2);
    assertEq(pair_max, 2);

    // Test double type.
    pair_min = Math.min(1.2, 2.3);
    assertEq(pair_min, 1.2);
    pair_max = Math.max(1.2, 2.3);
    assertEq(pair_max, 2.3);

    // Test float type.
    var expt_min = Math.fround(1.2);
    var expt_max = Math.fround(2.3);
    pair_min = Math.min(Math.fround(1.2), Math.fround(2.3));
    assertEq(pair_min, expt_min);
    pair_max = Math.max(Math.fround(1.2), Math.fround(2.3));
    assertEq(pair_max, expt_max);

    // The test cases for minmax with more than two operands.
    // Test integer type.
    pair_min = Math.min(1, 3, 2, 5, 4);
    assertEq(pair_min, 1);
    pair_max = Math.max(1, 3, 2, 5, 4);
    assertEq(pair_max, 5);

    // Test double type.
    pair_min = Math.min(1.1, 3.3, 2.2, 5.5, 4.4);
    assertEq(pair_min, 1.1);
    pair_max = Math.max(1.1, 3.3, 2.2, 5.5, 4.4);
    assertEq(pair_max, 5.5);

    // Test float type.
    expt_min = Math.fround(1.1);
    expt_max = Math.fround(5.5);
    pair_min = Math.min(Math.fround(1.1), Math.fround(3.3), Math.fround(2.2),
                        Math.fround(5.5), Math.fround(4.4));
    assertEq(pair_min, expt_min);
    pair_max = Math.max(Math.fround(1.1), Math.fround(3.3), Math.fround(2.2),
                        Math.fround(5.5), Math.fround(4.4));
    assertEq(pair_max, expt_max);
}

minmax();
minmax();
