var a = 1.1;
function f1() {
    return a + 0.2;
}
function test1() {
    for (var i=0; i<100; i++) {
        assertEq(f1(), 1.3);
    }
    a = 20;
    assertEq(f1(), 20.2);
}
test1();

function f2(arr) {
    return arr[2] + 0.2;
}
function test2() {
    var a = [1.1, 2.2, 3.3, 4.4];
    for (var i=0; i<100; i++) {
        assertEq(f2(a), 3.5);
    }
    a[2] = 123;
    assertEq(f2(a), 123.2);
}
test2();

function f3(arr, idx) {
    return arr[idx] + 0.2;
}
function test3() {
    var a = [1.1, 2.2, 3.3, 4.4];
    for (var i=0; i<100; i++) {
        assertEq(f3(a, 2), 3.5);
    }
    a[2] = 123;
    assertEq(f3(a, 2), 123.2);
}
test3();
