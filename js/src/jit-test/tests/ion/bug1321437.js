function f(idx) {
    "use strict";
    let z = [0, 1, 2, 3, 4, 5, 6, 7, 8, , , ];
    Object.freeze(z);
    try {
        z[idx] = 0;
    } catch (e) {
        return e.message;
    }
}
assertEq(f(4), "4 is read-only");
assertEq(f(-1), 'can\'t define property "-1": Array is not extensible');
assertEq(f(9), "can't define property 9: Array is not extensible");
assertEq(f(0xffffffff), 'can\'t define property "4294967295": Array is not extensible');
