// |jit-test| error: ReferenceError

for (let b in [0]) {
    let b = b ? 0 : 1
}
