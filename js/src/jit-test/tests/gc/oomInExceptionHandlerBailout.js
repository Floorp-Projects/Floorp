// |jit-test| skip-if: !('oomTest' in this)
oomTest(() => {
    let x = 0;
    try {
        for (let i = 0; i < 100; i++) {
            if (i == 99)
                throw "foo";
            x += i;
        }
    } catch (e) {
        x = 0;
    }
    return x;
});
