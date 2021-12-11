load(libdir + "eqArrayHelper.js");

var check = [];
function t(token) {
    check.push(token);
    return token;
}
let f = (...x) => x;
f(3, ...[t(1)], ...[t(2), t(3)], 34, 42, ...[t(4)]);
assertEqArray(check, [1, 2, 3, 4]);

var arr = [1, 2, 3];
assertEqArray(f(...arr, arr.pop()), [1, 2, 3, 3]);
