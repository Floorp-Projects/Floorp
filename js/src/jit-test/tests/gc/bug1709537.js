function f() {
    var obj = [];
    for (var count = 20000; count > 15900; count--) {
        obj[count] = 2;
    }
    assertEq(Object.getOwnPropertyNames(obj).length, 4101);
}
gczeal(4);
f();
