// |jit-test| error: TypeError: invalid assignment to const
function f() {
    const x60 = [0, 1];
    const y81 = [...x60];
    {
        const x60 = [0, 1];
        [...x60] = [42];
    }
}
f();
