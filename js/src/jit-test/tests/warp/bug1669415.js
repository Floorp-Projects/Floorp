function f(x, y) {
    return +(-y ? -x : (y ? x : NaN));
}
let arr = [false, {}, {}];
for (let i = 0; i < 9; ++i) {
    f(1.1, 2);
}
for (let i = 0; i < arr.length; i++) {
    output = f(true, arr[i]);
}
assertEq(output, 1);
