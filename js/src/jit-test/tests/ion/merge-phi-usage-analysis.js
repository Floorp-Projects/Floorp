
function expensive() {
    with({}) {}
}

function phi_merge_0(i) {
    // These computations can overflow, if the output is not truncated.
    i = i | 0;
    var a0 = i + i;
    var a1 = i + i;

    if ((a1 | 0) - ((2 * i) | 0)) {
        // Good candidate for branch pruning, which marks only a1 as having
        // removed uses.
        expensive();
        expensive();
        expensive();
        expensive();
        expensive();
    }

    // Simple branch made to let GVN merge the Phi instructions.
    if (a1 % 3 == 1) {
        a1 = 2 * i;
        a0 = 2 * i;
    }

    // a0 is never used, but a1 is truncated.
    return a1 | 0;
}

function phi_merge_1(i) {
    // These computations can overflow, if the output is not truncated.
    i = i | 0;
    var a1 = i + i;
    var a0 = i + i;

    if ((a1 | 0) - ((2 * i) | 0)) {
        // Good candidate for branch pruning, which marks only a1 as having
        // removed uses.
        expensive();
        expensive();
        expensive();
        expensive();
        expensive();
    }

    // Simple branch made to let GVN merge the Phi instructions.
    if (a1 % 3 == 1) {
        a1 = 2 * i;
        a0 = 2 * i;
    }

    // a0 is never used, but a1 is truncated.
    return a1 | 0;
}

for (var j = 0; j < 300; j++) {
    for (var i = 1; i == (i | 0); i = 2 * i + 1) {
        assertEq(phi_merge_0(i) < 0x80000000, true);
        assertEq(phi_merge_1(i) < 0x80000000, true);
    }
}
