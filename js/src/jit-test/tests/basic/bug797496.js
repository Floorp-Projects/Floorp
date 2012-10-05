// |jit-test| error: TypeError
var set = Set(['a']);
var n = 5;
for (let v of set) {
    if (n === 0)
        break;
    let g = set(Set(0xffffffff, n), 1);
}
