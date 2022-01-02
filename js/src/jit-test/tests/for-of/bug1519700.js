function* wrapNoThrow() {
    let iter = {
        [Symbol.iterator]() {
            return this;
        },
        next() {
            return {};
        },
        return () {}
    };
    for (const i of iter)
        yield i;
}
function foo() {
    l2: for (j of wrapNoThrow()) {
        for (i of [1, 2, 3])
            break l2;
        return false;
    }
}
try {
    foo();
} catch {}
