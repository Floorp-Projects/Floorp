var global = 1;

function test1(x) {
    global = 2;
    var k = global;
    global = x;
    global = x + 1;
    k = global + global;
    return k;
}

for (var i=0; i<60; i++) {
    assertEq(test1(i), i + 1 + i + 1);
}

function test2(x) {
    global = 2;
    var k = global;

    for (var i=0; i<10; i++) {
        k = global;
        global = i + x;
    }
    return k;
}

for (i=0; i<50; i++) {
    assertEq(test2(i), i + 8);
}
