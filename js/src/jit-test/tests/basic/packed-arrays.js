var isPacked = getSelfHostedValue("IsPackedArray");

function test() {
    var arr;

    // basic
    arr = [];
    assertEq(isPacked(arr), true);
    arr[0] = 0;
    assertEq(isPacked(arr), true);
    arr[2] = 2;
    assertEq(isPacked(arr), false);

    // delete
    arr = [1, 2, 3];
    assertEq(isPacked(arr), true);
    delete arr[1];
    assertEq(isPacked(arr), false);

    // setting .length
    arr = [1];
    arr.length = 0;
    assertEq(isPacked(arr), true);
    arr.length = 1;
    assertEq(isPacked(arr), false);

    // slice
    arr = [1, 2, , 3];
    assertEq(isPacked(arr), false);
    assertEq(isPacked(arr.slice(0, 2)), true);
    assertEq(isPacked(arr.slice(0, 3)), false);
    assertEq(isPacked(arr.slice(2, 3)), false);
    assertEq(isPacked(arr.slice(3, 4)), true);

    // splice
    arr = [1, 2, 3];
    assertEq(isPacked(arr.splice(0)), true);
    arr = [1, , 2];
    assertEq(isPacked(arr.splice(0)), false);
    arr = [1, , 2];
    assertEq(isPacked(arr.splice(0, 1)), true);
    assertEq(arr.length, 2);
    assertEq(isPacked(arr.splice(0, 1)), false);
    assertEq(arr.length, 1);
    assertEq(isPacked(arr.splice(0, 1)), true);
    assertEq(arr.length, 0);
    assertEq(isPacked(arr.splice(0)), true);
}
for (var i = 0; i < 5; i++) {
    test();
}
