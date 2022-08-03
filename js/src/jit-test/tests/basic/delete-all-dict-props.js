function f(o) {
    for (var i = 0; i < 100; i++) {
        o["a" + i] = i;
    }
    for (var i = 0; i < 100; i++) {
        delete o["a" + i];
    }
    o.x = 123;
    assertEq(Object.keys(o).length, 1);
}
f({});
f([]);
let obj = newObjectWithManyReservedSlots();
f(obj);
checkObjectWithManyReservedSlots(obj);
