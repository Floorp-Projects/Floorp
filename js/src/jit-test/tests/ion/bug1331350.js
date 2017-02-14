// |jit-test| --ion-eager
function optimize(a, b) {
    a = a | 0;
    b = b | 0;

    if ((a & 3) === 0) {
        a = a + 1 | 0
    }

    if ((a & 7) !== 0) {
        a = a + 1 | 0
    }

    return a + b | 0
}

for (var i=0; i<20; i++) {
    assertEq(optimize(4 | 0, 6 | 0), 12);
    assertEq(optimize(7 | 0, 11 | 0), 19);
}

function not_optimizable(a, b) {
    a = a | 0;
    b = b | 0;

    if ((a & 3) > 0) {
        a = a + 1 | 0
    }

    if ((a & 3) >= 0) {
        a = a + 1 | 0
    }

    if ((a & 7) < 0) {
        a = a + 1 | 0
    }

    if ((a & 7) <= 0) {
        a = a + 1 | 0
    }

    if ((b & 3) === 1) {
        b = b + 1 | 0
    }

    if ((b & 7) !== 3) {
        b = b + 1 | 0
    }

    return a + b | 0
}

for (var i=0; i<20; i++) {
    assertEq(not_optimizable(4 | 0, 6 | 0), 12);
    assertEq(not_optimizable(7 | 0, 11 | 0), 20);
}
