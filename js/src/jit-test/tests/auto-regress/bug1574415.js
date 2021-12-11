var err = 0;

for (let x of [0, Object.create(null)]) {
    try {
        (new Int8Array)[1] = x;
    } catch (e) {
        err += 1;
    }
}

assertEq(err, 1);
