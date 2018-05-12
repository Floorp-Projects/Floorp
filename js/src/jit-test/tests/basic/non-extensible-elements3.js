function testAdd() {
    var a = [];
    Object.preventExtensions(a);
    for (var i = 0; i < 20; i++)
        a[i] = i;
    assertEq(a.length, 0);

    a = [];
    Object.seal(a);
    for (var i = 0; i < 20; i++)
        a[i] = i;
    assertEq(a.length, 0);

    a = [];
    Object.freeze(a);
    for (var i = 0; i < 20; i++)
        a[i] = i;
    assertEq(a.length, 0);
}
testAdd();
testAdd();

function testSet() {
    var a = [1, 2, 3];
    Object.preventExtensions(a);
    for (var i = 0; i < 20; i++)
        a[2] = i;
    assertEq(a.toString(), "1,2,19");

    a = [1, 2, 3];
    Object.seal(a);
    for (var i = 0; i < 20; i++)
        a[0] = i;
    assertEq(a.toString(), "19,2,3");

    a = [1, 2, 3];
    Object.freeze(a);
    for (var i = 0; i < 20; i++)
        a[1] = i;
    assertEq(a.toString(), "1,2,3");
}
testSet();
testSet();

function testSetHole() {
    var a = [1, , 3];
    Object.preventExtensions(a);
    for (var i = 0; i < 30; i++)
        a[1] = i;
    assertEq(a.toString(), "1,,3");

    a = [1, , 3];
    Object.seal(a);
    for (var i = 0; i < 30; i++)
        a[1] = i;
    assertEq(a.toString(), "1,,3");

    a = [1, , 3];
    Object.freeze(a);
    for (var i = 0; i < 30; i++)
        a[1] = i;
    assertEq(a.toString(), "1,,3");
}
testSetHole();
testSetHole();
