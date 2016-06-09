if (!this.SharedArrayBuffer)
    quit();

function mod(stdlib) {
    add = stdlib.Atomics.add;
    function f3() {
        add(i8a, 0 | 0 && this && BUGNUMBER, 0);
    }
    return {f3: f3, 0: 0};
}
i8a = new Int8Array(new SharedArrayBuffer(1));
var {
    f3
} = mod(this, {})
for (i = 0; i < 1000; i++)
    f3();
