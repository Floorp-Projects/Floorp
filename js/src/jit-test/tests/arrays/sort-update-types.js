Object.setPrototypeOf(Array.prototype, {
    get 0() {
        Object.setPrototypeOf(Array.prototype, Object.prototype);
        return "159".repeat(5).substring(2, 5);
    }
});

var array = [
 /*0*/,  1,  2,  3,  4,  5,  6,  7,  8,  9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
];

array.sort();

gc();

var r = array[array.length - 1] * 1;
assertEq(r, 915);
