function add0_5 (n) {
    return n + 0.5;
}

function add1_0 (n) {
    return n + 1;
}

function add1_5 (n) {
    return n + 1.5;
}

function add2_0 (n) {
    return n + 2;
}

function sub2_0 (n) {
    return n - 2;
}


var num = 1.5;

function main () {
    for (var i = 0; i < 1000; i++) {
        assertEq(add0_5(num), 2);
        assertEq(add1_0(num), 2.5);
        assertEq(add1_5(num), 3);
        assertEq(add2_0(num), 3.5);
        assertEq(sub2_0(num), -0.5);
    }
}
main();
