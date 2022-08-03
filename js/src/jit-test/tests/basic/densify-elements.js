function f(o) {
    for (var i = 1; i <= 5; i += 4) {
        var start = i * 4000;
        var end = (start * 1.4)|0;
        for (var j = end; j > start; j--) {
            o[j] = j;
        }
    }
    assertEq(Object.keys(o).length, 9600);
}
f({});
f([]);
let obj = newObjectWithManyReservedSlots();
f(obj);
checkObjectWithManyReservedSlots(obj);
