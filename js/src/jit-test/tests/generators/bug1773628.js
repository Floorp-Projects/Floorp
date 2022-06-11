// |jit-test| --fast-warmup
function* f() {
    try {
        yield;
    } finally {
        for (let b = 0; b < 10; b++) {}
    }
}
for (var i = 0; i < 50; i++) {
    let c = f();
    c.next();
    c.return();
}
