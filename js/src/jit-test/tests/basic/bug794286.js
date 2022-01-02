// |jit-test| error: TypeError
for (var i = 0; i < 1; i++) {
    let y
    if (undefined) continue
    undefined.e
}
