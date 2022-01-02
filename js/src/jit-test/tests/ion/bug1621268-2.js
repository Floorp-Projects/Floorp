function f() {
    for (const x of []) {
       for (let y of [y, y]) {}
    }
}
f();
