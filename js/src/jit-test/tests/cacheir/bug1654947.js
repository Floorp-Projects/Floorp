function f() {
    let g = x => (x >>> {} >>> x, x);
    let arr = [0, 0xffff, undefined];
    for (let i = 0; i < 10; i++) {
        for (let j = 0; j < 3; j++) {
            let y = g(arr[j]);
            assertEq(y, arr[j]);
        }
    }
}
f();
