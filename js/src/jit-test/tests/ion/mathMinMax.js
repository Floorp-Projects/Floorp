var nan = Number.NaN;
var negative_zero = -0;

function max(a, b) {
    return Math.max(a, b);
}
function min(a, b) {
    return Math.min(a, b);
}

function main() {
    for (var i = 0; i < 100; i++) {
        assertEq(max(negative_zero, 0), 0);
        assertEq(max(0, negative_zero), 0);
        assertEq(min(0, negative_zero), negative_zero);
        assertEq(min(negative_zero, 0), negative_zero);

        assertEq(min(negative_zero, negative_zero), negative_zero);
        assertEq(max(negative_zero, negative_zero), negative_zero);

        assertEq(max(nan, 0), nan);
        assertEq(min(nan, 0), nan);

        assertEq(max(0, nan), nan);
        assertEq(max(nan, 0), nan);

        assertEq(max(3, 5), 5);
        assertEq(max(5, 3), 5);

        assertEq(min(3, 5), 3);
        assertEq(min(5, 3), 3);

        assertEq(max(Infinity, -Infinity), Infinity);
        assertEq(min(Infinity, -Infinity), -Infinity);
        assertEq(max(Infinity, nan), nan);

        assertEq(max(negative_zero, -5), negative_zero);
        assertEq(min(negative_zero, -5), -5);
    }
}

main();