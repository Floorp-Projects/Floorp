function test1() {
    var dense = [1, 2, 3];
    var denseHoles = [1, , 3];
    var result = 0;

    for (var i = 0; i < 70; i++) {
        if (i in dense) result += 1;
        if (1 in dense) result += 2;
        if (3 in dense) result += 3;
        if (-1000 in dense) result += 4;
        if (i in denseHoles) result += 5;
        if (1 in denseHoles) result += 6;
    }

    assertEq(result, 153);
}
test1();

function test2() {
    var a = [1, 2, 3];

    for (var i = 0; i < 70; i++) {
        assertEq(-0 in a, true);
        assertEq(Math.sqrt(4) in a, true);
        assertEq(1.9 in a, false);
        assertEq(NaN in a, false);
        assertEq(Infinity in a, false);
    }
}
test2();

function test3() {
    var a = [1, , 3];

    for (var i = 0; i < 70; i++) {
        if (i == 60)
            Object.prototype[1] = null;
        assertEq(1 in a, i >= 60);
    }
}
test3();